defmodule PHP.Controller do
  use GenServer

  # require Logger

  @nodes_count Application.compile_env!(:php_ex, :nodes_count)
  @secretcookie Application.compile_env!(:php_ex, :secretcookie)

  @priv_dir "#{File.cwd!()}/priv" # Application.app_dir(:php_ex, "priv")


  ################################################################################################
  ## Public API
  ################################################################################################
  def start_link(_) do
    GenServer.start_link(__MODULE__, [], name: __MODULE__)
  end

  def run(script_path, get \\ [], post \\ [], cookie \\ []) do
    {node, rid} = GenServer.call(__MODULE__, {:run, self(), {script_path, get, post, cookie}})
    receive do
      {^node, ^rid, :error, msg} ->
        {:error, msg}
      {^node, ^rid, body, headers} ->
        {:ok, body, headers}
      msg ->
        {:error, :unexpected_message, msg}
    end
  end

  ################################################################################################
  ## Private
  ################################################################################################
  defp loop(node_name) do
    spawn(fn ->
      System.cmd(
        "#{@priv_dir}/php_node",
        ["#{node_name}", "#{@secretcookie}"] #, into: IO.stream()
      )
      loop(node_name)
    end)
  end
  
  ################################################################################################
  ## Callbacks
  ################################################################################################
  @impl true
  def init(_) do    
    nodes =
      for n <- 1..@nodes_count do
        node_name = "php#{n}"
        loop(node_name)
        String.to_atom("#{node_name}@127.0.0.1")
      end

    status = %{
      nodes: CList.new(nodes)
    }

    {:ok, status}
  end

  @impl true
  def handle_call({:run, pid, params}, _from, %{nodes: nodes} = status) do
    rid = 4 |> :crypto.strong_rand_bytes() |> :binary.decode_unsigned()
    {node, nodes} = CList.next(nodes)
    send({:php_master, node}, {pid, rid, :run, params})
    {:reply, {node, rid}, %{status | nodes: nodes}}
  end

  @impl true
  # def handle_info({_, {:data, {_, line}}}, status) do
  #   # Logger.log(:info, "#{inspect p} #{line}")
  #   IO.puts(line)
  #   {:noreply, status}
  # end
  def handle_info(msg, status) do
    IO.inspect msg, label: "UNKNOWN MSG"
    {:noreply, status}
  end

  @impl true
  def terminate(_reason, %{nodes: nodes}) do
    nodes
    |> CList.to_list()
    |> Enum.each(fn node ->
      send({:any, node}, {self(), 0, :exit, nil})
    end)
  end
end
