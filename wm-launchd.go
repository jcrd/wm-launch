/*
wm-launch - launch X11 clients with unique IDs
Copyright (C) 2019-2020 James Reed

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

package main

import (
    "bufio"
    "flag"
    "fmt"
    "log"
    "net"
    "os"
    "path/filepath"
    "strconv"
    "strings"
    "sync"
    "time"
)

type Factory struct {
    pid int
    timestamp time.Time
    mutex *sync.Mutex
    ids []string
}

func (f *Factory) exists() bool {
    t, err := getPidTimestamp(f.pid)
    if err != nil {
        return false
    }
    return t == f.timestamp
}

func (f *Factory) addID(id string) {
    f.mutex.Lock()
    f.ids = append(f.ids, id)
    f.mutex.Unlock()
}

func (f *Factory) popID() (string, error) {
    n := len(f.ids)
    if n > 0 {
        f.mutex.Lock()
        id := f.ids[0]
        if n > 1 {
            f.ids = f.ids[1:]
        } else {
            f.ids = []string{}
        }
        f.mutex.Unlock()
        return id, nil
    }
    return "", fmt.Errorf("factory has no IDs")
}

type FactMap struct {
    mutex *sync.RWMutex
    facts map[string]*Factory
}

func (fm *FactMap) add(name string, pid int) (*Factory, error) {
    var err error
    f := &Factory{pid: pid, timestamp: time.Time{}, mutex: &sync.Mutex{}}
    if pid > 0 {
        f.timestamp, err = getPidTimestamp(pid)
        if err != nil {
            return nil, fmt.Errorf("ERROR PID")
        }
    }

    fm.mutex.Lock()
    fm.facts[name] = f
    fm.mutex.Unlock()

    return f, nil
}

func (fm *FactMap) remove(name string) {
    fm.mutex.Lock()
    delete(fm.facts, name)
    fm.mutex.Unlock()
}

func (fm *FactMap) get(name string) *Factory {
    fm.mutex.RLock()
    defer fm.mutex.RUnlock()
    if f, ok := fm.facts[name]; ok {
        return f
    }
    return nil
}

func (fm *FactMap) check(name string) bool {
    f := fm.get(name)

    if f == nil {
        return false
    }
    if f.exists() {
        return true
    } else {
        fm.remove(name)
    }
    return false
}

func (fm *FactMap) getFactNames() []string {
    names := make([]string, 0, len(fm.facts))
    fm.mutex.RLock()
    defer fm.mutex.RUnlock()
    for n := range fm.facts {
        names = append(names, n)
    }
    return names
}

var factMap = FactMap{
    mutex: &sync.RWMutex{},
    facts: make(map[string]*Factory),
}
var socketPath = getSocketPath()
var _, logDebug = os.LookupEnv("WM_LAUNCHD_DEBUG")

func debug(fmt string, s ...interface{}) {
    if logDebug {
        log.Printf("wm-launchd: " + fmt + "\n", s...)
    }
}

func getSocketPath() string {
    display := os.Getenv("DISPLAY")
    if len(display) == 0 {
        log.Fatal("DISPLAY not set")
    }

    rundir := os.Getenv("XDG_RUNTIME_DIR")
    if len(rundir) == 0 {
        log.Fatal("XDG_RUNTIME_DIR not set")
    }

    path := fmt.Sprintf("%s/wm-launch/%s.sock", rundir, display)
    debug("Socket path: %s", path)

    return path
}

func getPidTimestamp(pid int) (time.Time, error) {
    pidfile, err := os.Open(fmt.Sprintf("/proc/%d", pid))
    if err != nil {
        return time.Time{}, fmt.Errorf("pid %d not found", pid)
    }
    defer pidfile.Close()

    info, err := pidfile.Stat()
    if err != nil {
        return time.Time{}, err
    }

    return info.ModTime(), nil
}

func handleConn(c net.Conn) {
    defer c.Close()

    reply := func(s ...interface{}) { fmt.Fprintln(c, s...) }

    msg, err := bufio.NewReader(c).ReadString('\n')
    if err != nil {
        log.Fatal(err)
    }

    fields := strings.Fields(msg)
    debug("Received command: %s", fields)

    switch fields[0] {
    case "REGISTER_FACTORY":
        var err error
        pid, err := strconv.Atoi(fields[2])
        if err != nil {
            log.Fatal(err)
        }
        name := fields[1]
        if f := factMap.get(name); f != nil {
            f.pid = pid
            f.timestamp, err = getPidTimestamp(pid)
        } else {
            _, err = factMap.add(name, pid)
        }
        if err != nil {
            reply(err.Error())
            break
        }
        reply("OK")
    case "REMOVE_FACTORY":
        name := fields[1]
        if factMap.get(name) == nil {
            reply("ERROR NO_FACTORY")
            break
        }
        factMap.remove(name)
        reply("OK")
    case "LIST_FACTORIES":
        names := factMap.getFactNames()
        if len(names) == 0 {
            reply("ERROR NO_FACTORY")
            break
        }
        reply(strings.Join(names[:], " "))
    case "CHECK_FACTORY":
        if factMap.check(fields[1]) {
            reply("TRUE")
        } else {
            reply("FALSE")
        }
    case "LIST_IDS":
        f := factMap.get(fields[1])
        if f == nil {
            reply("ERROR NO_FACTORY")
            break
        }
        if len(f.ids) == 0 {
            reply("ERROR NO_IDS")
            break
        }
        reply(strings.Join(f.ids[:], " "))
    case "ADD_ID":
        var err error
        name := fields[1]
        f := factMap.get(name)
        if f == nil {
            f, err = factMap.add(name, -1)
            if err != nil {
                reply(err.Error())
                break
            }
        }
        f.addID(fields[2])
        reply("OK")
    case "GET_ID":
        f := factMap.get(fields[1])
        if f == nil {
            reply("ERROR NO_FACTORY")
            break
        }
        id, err := f.popID()
        if err != nil {
            reply("ERROR NO_ID")
            break
        }
        reply("ID", id)
    }
}

func send(s ...interface{}) string {
    c, err := net.Dial("unix", socketPath)
    if err != nil {
        log.Fatal(err)
    }

    debug("Sending message: %s", s)
    fmt.Fprintln(c, s...)

    rsp, err := bufio.NewReader(c).ReadString('\n')
    if err != nil {
        log.Fatal(err)
    }
    rsp = strings.Trim(rsp, "\n")
    debug("Response: %s", rsp)

    return rsp
}

func main() {
    var factory string
    var id string
    var check string
    var list bool

    printList := func(cmd ...interface{}) {
        if list {
            rsp := send(cmd...)
            if strings.HasPrefix(rsp, "ERROR") {
                os.Exit(1)
            }
            fmt.Println(rsp)
            os.Exit(0)
        }
    }

    checkFact := func(name string) {
        if send("CHECK_FACTORY", name) == "TRUE" {
            os.Exit(0)
        } else {
            os.Exit(1)
        }
    }

    flag.StringVar(&factory, "factory", "", "name of window factory")
    flag.StringVar(&id, "id", "", "ID to add to factory")
    flag.StringVar(&check, "check", "", "name of factory to check")
    flag.BoolVar(&list, "list", false, "list factory names or IDs")

    flag.Parse()

    if check != "" {
        checkFact(check)
    }

    if factory != "" {
        printList("LIST_IDS", factory)
        if id != "" {
            send("ADD_ID", factory, id)
            os.Exit(0)
        }

        checkFact(factory)
    }

    printList("LIST_FACTORIES")

    if err := os.RemoveAll(socketPath); err != nil {
        log.Fatal(err)
    }

    dir := filepath.Dir(socketPath)
    if err := os.MkdirAll(dir, os.ModePerm); err != nil {
        log.Fatal(err)
    }

    s, err := net.Listen("unix", socketPath)
    if err != nil {
        log.Fatal(err)
    }
    defer s.Close()

    for {
        c, err := s.Accept()
        if err != nil {
            log.Fatal(err)
        }
        go handleConn(c)
    }
}
