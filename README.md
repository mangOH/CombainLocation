# CombainLocation

This repository provides two Legato apps. combainLocation.adef provides a service for converting
radio scan results into a location using an HTTP API provided by combain.com. combainCli.adef is a
simple command line application that performs a WiFi scan and then uses the service app to resolve a
location.

## Instructions
1. `config set combainLocation:/ApiKey [YOUR_API_KEY_FROM_COMBAIN.COM]`
1. Include the apps in your .sdef and also add to your `commands` section like this:
```
commands:
{
    combain = combainCli:/bin/combain
}
```
1. Ensure that your target has a connection to the Internet. Either use `cm data connect` or
   something similar. The app will not attempt to bring up a connection on its own.
1. Run `combain -w` on the target to initiate a WiFi scan and resolve a location.

## Limitations
* Currently only WiFi is supported, by the Legato service, but combain.com supports many other scan
  types.
* Many apps can bind to the service and build up independent requests simultaneously, but only one
  request will be in flight at a time. In practice, this shouldn't be a problem for an embedded
  device.
