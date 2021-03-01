# mllp-activemq

Simple program to listen for HL7 MLLP requests on a local port, send them
verbatim as a message to an ActiveMQ server, then acknowledge the message to
the sender.

## Local buffering

If you use the -L flag to specify a local queue directory, each message will
be acknowledge as soon as it is written to the local directory and synced.
A write-behind thread then does the actual push to the MQ server and deletes
the file if successful.

## Flags

-S &lt;activemq URI&gt;

-U &lt;activemq username&gt;

-P &lt;activemq password&gt;

-L &lt;local queue directory&gt;

-Q &lt;destination queue name&gt;

## To-Do

Deal with HL7 V3.  I've structured the classes to be able to handle that, but
I haven't actually run into anyone using HL7 V3 in the wild.

Automake isn't checking for the OpenSSL dev package right.  I'm not the
wizard of automake so I'm sure there are all kinds of things wrong there.

Other bugs no doubt.

