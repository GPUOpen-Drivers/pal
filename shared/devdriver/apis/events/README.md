# DDEvent Versioning

Each event provider is versioned with two 16-bit integers,
`DDEventProviderHeader::versionMajor` and `DDEventProviderHeader::versionMinor`.
All events of the same provider share the same version. If any event has a
major or minor version change, the provider's version numbers should be
incremented accordingly.

Minor version changes are considered backwards compatible, whereas major versions are
treated as breaking changes. In other words, with a minor version change, users of
this API don't need to change/re-compile their code in order to parse new data.
However, with a major version bump, users are required to update/re-compile their code
to parse new data.

## Modify Event Definitions

### Minor Version Changes

- Appending a field to a struct definition.
- Making use of previously reserved bits.

Note, changing the name of a struct or field is neither minor nor major change.

### Major Version Changes

Anything not mentioned in "Minor Version Changes".

### Preserve the Old Versions of Event Definitions

When a major version change is introduced to an event defintion, besides incrementing
`DDEventProviderHeader::versionMajor`, a copy of old event definition should be
made with its name suffixed with the old version number.

For example, assumming the event provider `AlphabetEvents` has two events
`EventA` and `EventB`, and the current major version is 0.

```C++
struct EventA { ... };
struct EventB { ... };
```

If a major version change is made to `EventA`, `AlphabetEvents::versionMajor`
should be bumped to 1, and event definitions should be updated to:

```C++
struct EventA { ... };
struct EventA_v0 { ... };
struct EventB { ... };
```

Later, another major version change is made to `EventB`, `AlphabetEvents::versionMajor`
should be bumped to 2, and event definitions should be updated to:

```C++
struct EventA { ... };
struct EventA_v0 { ... };
struct EventB { ... };
struct EventB_v0_v1 { ... };
```

The `_v0_v1` part signals that `EventB` didn't change between v0 and v1 of
`AlphabetEvents`.

## Parsing Event Data

It is advised to check for newer versions of event definitions at the top of
the parsing code, and error out with clear message to prompt users to update their
parser.

```C++
// `providerHeader.versionMajor` is extracted from an event stream.
// `AlphabetEvents::VersionMajor` comes from the event header file the parsing code `#include`s.
if (providerHeader.versionMajor > AlphabetEvents::VersionMajor) {
    // The provider version in the event stream is greater than the parser.
    // Error out to let users know that they need to update the parser.
    return ParserNotUpToDate;
}
```

To provide backwards compatibility, parsing code can check provider versions
for individual events. For example, let's say the current major version of
`AlphabetEvents` is 0. Since it's the first version, events must all be of
version 0. The parsing code would look something like the following:

```C++
switch (eventHeader.eventId) {
case AlphabetEvents::EventId::EventA:
    EventA event = {};
    event.FromBuffer(stream);
    break;

case AlphabetEvents::EventId::EventB:
    EventB event = {};
    event.FromBuffer(stream);
    break;
...
}
```

Now let's say we made a major version change to `EventA`, but nothing changed
for `EventB`. The parsing code should be updated:

```C++
switch (eventHeader.eventId) {
case AlphabetEvents::EventId::EventA:
    if (providerHeader.versionMajor >= 1) {
        // The current parser only knows 2 versions of `AlphabetEvents`.
        // Any version greater than 1 should already be detected at the top of
        // the parsing code and errored out.
        EventA event = {};
        event.FromBuffer(stream);
    } else {
        // We introduced a major version change, and bumped the major version
        // number to 1. The old event definition is preserved but its name
        // is suffixed with the old version number.
        EventA_v0 event = {};
        event.FromBuffer(stream);
    }
    break;

case AlphabetEvents::EventId::EventB:
    EventB event = {};
    event.FromBuffer(stream);
    break;
...
}
```

Later, another major version change is made to `EventB`. The parsing code
should be update:

```C++
switch (eventHeader.eventId) {
case AlphabetEvents::EventId::EventA:
    if (providerHeader.versionMajor >= 1) {
        // The current parser knows 3 versions of `AlphabetEvents`.
        // V1 and V2 of `EventA` are the same.
        EventA event = {};
        event.FromBuffer(stream);
    } else {
        EventA_v0 event = {};
        event.FromBuffer(stream);
    }
    break;

case AlphabetEvents::EventId::EventB:
    if (providerHeader.versionMajor >= 2) {
        EventB event = {};
        event.FromBuffer(stream);
    } else if (providerHeader.versionMajor >= 0 &&
               providerHeader.versionMajor <= 1) {
        EventB_v0_v1 event = {};
        event.FromBuffer(stream);
    } else {
        Unreacheable();
    }
    break;
...
}
```
