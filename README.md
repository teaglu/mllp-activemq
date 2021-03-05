# mllp-activemq

Simple program to listen for HL7 MLLP requests on a local port, send them
verbatim as a message to an ActiveMQ server, then acknowledge the message to
the sender.

## Local buffering

If you use the -L flag to specify a local queue directory, each message will
be acknowledge as soon as it is written to the local directory and synced.
A write-behind thread then does the actual push to the MQ server and deletes
the file if successful.

## Message Headers

All messages have a "MLLP-Timestamp" header which contains the ISO8601 time
when the message was recieved, and a "MLLP-RemoteHost" header which contains
the IP address the message was received from.

## JSON Envelope

The -j flag enables wrapping the message body using JSON.  The JSON object
sent will include a "message" member holding the original member, a
"timestamp" member with the ISO8601 time when the message was originally
received, and a "remoteHost" member with the IP address the message was
received from.

Without the -j flag, the message body will be composed of only the HL7 message
body.

## Acknowledgements

This program replies with an appropriate HL7 acknowledgement message for each
message received.  An AA is sent if the message is successfully queued, and
AE is sent if the message was unable to be queued, and an AR is sent if the
message is not syntactically valid.

The message is not exhaustively validated, as this program is meant to be a
lightweight proxy suitable for embedding in SoC computers (hence the C++).
The only checks done are to make sure that enough information can be extracted
from the message header to reply appropriately.

If MLLP protocol is not followed correctly, the program logs the error and
terminates the connection.

## IPv6 Support

This program opens a separate listening socket to natively support IPv6.

## SSL Peer Validation

The -i flag passes tells the ActiveMQ library to not perform standard peer
validation.  This allows self-signed certificates, and may be necessary for
wildcard certificates.

## Flags

Configuration is done by command line flags, and if the command line flags
are missing configuration then tries the environment variables listed below.

These flags can be used:

| Flag            | Setting                                 |
| --------------- | --------------------------------------- |
| -p {port}       | TCP Port Number to Listen On            |
| -S {URI}        | ActiveMQ Server URI / Connection String |
| -U {Username}   | ActiveMQ Connection Username            |
| -P {Password}   | ActiveMQ Connection Password            |
| -Q {Queue Name} | ActiveMQ Queue to Send To               |
| -L {Path}       | Local Directory for Store/Forward Mode  |
| -j              | Enable JSON Envelope                    |
| -i              | Disable SSL Peer Validation             |

## Environment Variables

Some variable can also be specified by using an environment variable, which
is typically more convenient to a docker environment.

These environment variables can be used:

| Variable        | Setting                                 |
| --------------- | --------------------------------------- |
| AMQ_URI         | ActiveMQ Server URI / Connection String |
| AMQ_USERNAME    | ActiveMQ Connection Username            |
| AMQ_PASSWORD    | ActiveMQ Connection Password            |
| AMQ_QUEUE       | ActiveMQ Queue to Send To               |
| LOCALQUEUE_PATH | Local Directory for Store/Forward Mode  |

## To-Do

Deal with HL7 V3.  I've structured the classes to be able to handle that, but
I haven't actually run into anyone using HL7 V3 in the wild.

Automake isn't checking for the OpenSSL dev package right.  I'm not the
wizard of automake so I'm sure there are all kinds of things wrong there.

Other bugs no doubt.

