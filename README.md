<h2>Work in process</h2>

# PHP_EX (A project just for fun)

`PHP_EX` is an interface that allow to run PHP scripts inside Elixir. `PHP_EX` can run PHP code 
thanks to Erlang C-Nodes running in background waiting for requets to run a script. The hard work 
is done by the C-Nodes; every one, when receive a request, create a forked process, run the 
script and send the result to the request caller. 

## Features
  - You can define the amount of C-Nodes running.
  - Simple public API, just one function: `run(...)`
  - In a request you can send `GET` params, `POST` params and `COOKIEs`; in this way you can make 
    think to the script that it is running inside Apache Web Server (or any other Web Server). 
  - You can integrate `PHP_EX` with `Cowboy` or another Elixir/Erlang Web Server to put into 
    operation your old PHP app/site. 
  - `PHP_EX` implement a module called `PHP.Controller`. This module (a `GenServer`) and is 
    supervised by the main process (Application). `PHP.Controller` takes care if start the C-Nodes
    and restart them in case of an unexpected exit. `PHP.Controller`, to keep a balanced use of 
    the C-Nodes, use a round robin strategy to derive every request received. 

## TODO: 

- [ ] Improve the web server simulation to deceive PHP script. 
- [ ] Improve the `PHP.Controller` strategy to manage request (keep track of running scripts, 
      maybe increase the amount of C-Nodes when the load grow, etc). 

## Installation

```elixir
def deps do
  [
    {:php_ex, "~> 0.1.0"}
  ]
end
```

