defmodule PHP.Application do
  @moduledoc false
  use Application

  @secretcookie Application.compile_env!(:php_ex, :secretcookie)
  
  @impl true
  def start(_type, _args) do
    Node.start(:"php_master@127.0.0.1")
    
    if Node.alive?() do
      Node.set_cookie(@secretcookie)
      children = [
        PHP.Controller
      ]

      opts = [strategy: :one_for_one, name: PHP.Supervisor]
      Supervisor.start_link(children, opts)
    else
      {:error, "Node not initilized"}
    end
  end

  @impl true
  def stop(_) do
    GenServer.stop(PHP.Controller, :shutdown)
  end
end
