import Config

config :logger, :default_formatter,
       format: "$time - [$level] $message $metadata\n"

config :php_ex,
       nodes_count: 5,
       secretcookie: :supersecret
