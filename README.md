# My solution to the Operation Wirestorm Challenge by CoreTech

The challenge is visible here: https://www.coretechsec.com/operation-wire-storm

# Why did I decide to do this?
- I like a challenge!
- I had a very in-depth conversation with one of the CoreTech reps at a Careers Fair earlier this year, and came away very happy with the level of detail I got from the rep about various aspects, it was the most engaging conversation I had at that fair!
- There are parallels between this challenge and the coursework for Warwick's CS241 Operating Systems and Computer Networks module, which I found enjoyable
- I am having a crack at this challenge quite late on partly due to various other commitments over this summer - the time constraint and previous strongest familiarity with C inclined me towards it, rather than something like Rust, which I've only dabbled in so far
  - *And after all, I wouldn't want to write potentially insecure/sub-optimal code in a language I am building basic familarity with!*

# Development environment
- Ubuntu 24.04.3
- gcc 13.3.0 via `make clean` & `make` as defined in the `Makefile`, with an executable `proxy` created
- Using `proxy`:
  - just `proxy` will attempt use `127.0.0.1` with the `source` listener on port `33333` and destination listener on port `44444`
  - these can be configured with `-i`, `-s`, `-d` e.g. `proxy -i 127.0.0.2 -s 12345 -d 23456`
  - `proxy` has the ability to calculate and validate checksums, as per stage 2 of the challenge
    - Stage 1 messages are compatible with the Stage 2 implementation
    - Stage 2 messages are not compatible with the Stage 1 implementation
      - This is due to my opting to have strict enforcement of e.g. padding being filled with 0s
  - `proxy` expects all source clients to conform with the CTMP protocol, and will disconnect any that do not, similarly cleaning up for "dead"/abnormally behaving destinations.

# Rough Development Process
- I decided to break this down into more basic milestones so that I can both learn and test at each step with my own chucked together scripts.
- After having done enough reading through various bits of documentation, I decided the most sensible and safe way for handling protocol violations in the scope of this challenge is to disconnect the client at the point of detection - I rationalised in the end that there's rarely ever a reasonable way to recover a connection after an incorrectly reported header length, malformed header etcetera, at which point the model becomes one of a "contract of trust" whereby it is on the source client to conform to the protocol of the proxy. If the source violates this contract, and especially due to the structure of the messages, the connection is left in an unpredictable state, and therefore inherently cannot be treated as secure.
- Roughly, these steps turned out as:
  - Start with a blocking model
  - Figure out connecting a single source
  - Read data from a single source
  - Parse & validate headers
  - Sending a single message to a single destination
  - Sending multiple messages to single destination
  - Allow multiple destinations through a simple "connection phase" model
  - Multiple messages to multiple destinations
  - Attempting to move to a non-blocking model using `epoll`
    - *Fun to learn, but slightly more difficult when very sleep-deprived! However, callgrind is my beloved when strange things like a spinning of an epoll_wait loop happens :)*
  - I'm writing this with half an hour to go, but checksums!
  - Tidying up, covering docs gaps, implementing some configuration options (e.g. for custom IP, src & dst)

# Other potential improvements
- Tracking free destination slots better
- Better write buffers e.g. possibly ring buffers?
- Configuration for ~~interface, src & dst listener port~~ (just done), max_dsts
- IPv6
- Formal test suite - *rather than just my hastily hacked together Python scripts :)*

# Well, it's done!
~~The all-nighters shall be no more~~

This has been a very fun challenge to do, admittedly very time pressured having gone from the start of this week :)