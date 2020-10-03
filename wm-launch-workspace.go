package main

import (
    "bufio"
    "fmt"
    "os"
    "path/filepath"

    "github.com/godbus/dbus/v5"
)

func main() {
    if len(os.Args) < 2 {
        fmt.Fprintln(os.Stderr, "requires workspace path argument")
        os.Exit(2)
    }

    path, err := filepath.Abs(os.Args[1])
    if err != nil {
        fmt.Fprintln(os.Stderr, err)
        os.Exit(2)
    }
    dir := filepath.Dir(path)

    file, err := os.Open(path)
    if err != nil {
        fmt.Fprintln(os.Stderr, err)
        os.Exit(2)
    }
    defer file.Close()

    var clients []string
    scanner := bufio.NewScanner(file)
    for scanner.Scan() {
        clients = append(clients, scanner.Text())
    }

    conn, err := dbus.SessionBus()
    if err != nil {
        fmt.Fprintln(os.Stderr, err)
        os.Exit(1)
    }
    defer conn.Close()

    obj := conn.Object("com.github.jcrd.wm_launch",
        "/com/github/jcrd/wm_launch/WindowManager")
    call := obj.Call("com.github.jcrd.wm_launch.WindowManager.Workspace", 0,
        filepath.Base(dir), dir, clients)
    if call.Err != nil {
        fmt.Fprintln(os.Stderr, call.Err)
        os.Exit(1)
    }
}
