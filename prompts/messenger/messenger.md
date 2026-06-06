## Overview

You are in the monorepo which conatins code for the backend service of a web-application that simplifies and streamlines hr and accounting processes of a small or medium sized company.
Companies are put into their own workspaces so there's no shared info between companies and every employee has their own profile in the system.

The service implements a bunch of features that are already used in production including task tracking and document management. Now we want to add a new functionality which is a messenger. The implementation is already there but it's very raw and untested, also potentially missing some features that we want in the end product.

The main goal is to make the messenger production-ready, test all of its features and create a simple web interface to be able to manually test the messenger.

## Code
The service is built using an asynchronous framework userver (documentation is in https://userver.tech/de/d6a/md_en_2index.html).
OpenAPI spec of all existing functionality is located at @docs/yaml/openapi.yaml
The config of the service is at @configs/static_config.yaml, note that it's in the userver-specific format.
Every API call should be authorized using an authorization token.
All the code is located at @src/ (use 'messenger' keyword to identify messenger-related code)

## Messenger requirements
- For every user we want to be able to have different chats, that could be direct messages with other employees of the same company or group chats with them.
- When opening a chat, a user should see previous history and be able to write a message that will be delivered to all the other chat participants.

## Implementation details
- For persisting chat information we are using Postgres
- For real-time message delivery the service is based on web-sockets

## Building and Testing
Tests are located at @tests/test_basic.py
'make build-release' -- building a service binary
'make test-release' -- building and running all tests


## Plan
I want you to spin up the service and write a simple web-interface that will be able to interact with the messenger. I don't want a complicated UI, just a simple one that is able to test the complete scope of the messenger features and requirements.
**If at any point you notice that there is some bug in the messenger backend or that it simply doesn't work, go and fix it.**
1. Spin up the local postgres database, apply the schemas to it and fill it with some test data. You can look at how we do it for testing but here the database should be kept alive instead of teminating after a testing session.
2. Spin up the service binary in the background (regular binary run is 'build_release/working_day -c configs/static_config.yaml --config_vars configs/config_vars.yaml')
3. Write a script for a simple messenger interaction -- just making sure that setup from steps 1 and 2 works.
4. Write python tests for messenger to @tests/test_basic.py in order to make sure that it works as intended.
5. Build a simple web app that implements a comprehensive messenger interaction that I can access from localhost and test.

## Instructions
While implementing, make sure to go one step at a time and document all non-trivial observations that you made during the implentation
