defmodule PHPTest do
  use ExUnit.Case
  doctest PHP

  test "greets the world" do
    assert PHP.hello() == :world
  end
end
