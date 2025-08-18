# My solution to the Operation Wirestorm Challenge by CoreTech

The challenge is visible here: https://www.coretechsec.com/operation-wire-storm

# Why did I decide to do this?
- I like a challenge!
- I had a very in-depth conversation with one of the CoreTech reps at a Careers Fair earlier this year, and came away very happy with the level of detail I got from the rep about various aspects, it was the most engaging conversation I had at that fair!
- There are parallels between this challenge and the coursework for Warwick's CS241 Operating Systems and Computer Networks module, which I found enjoyable
- I am having a crack at this challenge quite late on due to various other commitments over this summer - the time constraint and previous strongest familiarity with C inclined me towards it, rather than something like Rust, which I've only dabbled in so far
  - *And after all, I wouldn't want to write potentially insecure/sub-optimal code in a language I am building basic familarity with!*

# Development environment
- Ubuntu 24.04.3
- gcc 13.3.0 via `make clean` & `make` as defined in the `Makefile`, with an executable `proxy` created