<h2>Work in process</h2>

# PHP_EX (A project just for fun)

`PHP_EX` allow to run PHP scripts inside Elixir. `PHP_EX` can run PHP code thanks to Erlang 
C-Nodes running in background waiting for requets to run a script. The hard work 
is done by the C-Nodes; every one, when receive a request, create a forked process, run the 
script and send the result to the request caller. 

## Features
  - You can define the amount of C-Nodes running.
  - Simple public API, just one function: `run(...)`
  - In a request you can send `GET` params, `POST` params and `COOKIEs`; in this way you can make 
    think to PHP that it is running inside Apache Web Server (or any other Web Server). 
  - You can integrate `PHP_EX` with `Cowboy` or another Elixir/Erlang Web Server to put into 
    operation your old PHP app/site. 
  - `PHP_EX` implement a module called `PHP.Controller`. This module (a `GenServer`) is 
    supervised by the main process (Application) and restarted in case of crash. `PHP.Controller`
    takes care of start the C-Nodes and restart them in case of an unexpected exit. 
    `PHP.Controller`, to keep balanced the uses of the C-Nodes, implement a round robin strategy 
    to derive every new request received. 

## TODO: 
 
- [ ] Improve the web server simulation to deceive PHP script. 
- [ ] Improve the `PHP.Controller` strategy to manage request (keep track of running scripts, 
      maybe increase the amount of C-Nodes when the load grow, etc). 
- [ ] Integrate with my other toy project, [CORD Framework](https://github.com/mailcmd/cord-framework).

## Installation

```elixir
def deps do
  [
    {:php_ex, "~> 0.1.0"}
  ]
end
```

