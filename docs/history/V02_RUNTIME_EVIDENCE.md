# v0.2 runtime-service evidence

This is a historical evidence record for the bounded post-EOI runtime service. It
does not describe the current repository head or replace `../RUNTIME_SERVICE.md`.

## Implemented boundary

The v0.2 work moved GUI, input, USB, and network activity out of the fixed timer
callback into one service pass after interrupt-controller EOI and before process
dispatch and `eret`.

The promoted contract included:

- independent PERIODIC, INPUT, and NETWORK readiness;
- bounded virtio-input descriptor consumption;
- a four-device USB HID scan;
- a 16-event shared-input budget;
- an eight-rectangle partial-redraw budget;
- a 16-frame virtio-net RX budget;
- one service-wide generic-counter deadline checked at safe boundaries;
- conservative readiness republication on deadline expiry;
- strict suppression of network receive outside NETWORK phase;
- telemetry for work, duration, exhaustion, queue pressure, redraw shape, and
  software-visible network progress.

Issue #43 tracked this work and was closed as completed.

## Forced-expiry evidence

A dedicated test image forced one expiry every eight service passes while real
input, redraw, DHCP, network, and EL0 heartbeat work continued.

Recorded result:

```text
EL0 heartbeat markers:        509
deadline republish markers:   311
input-consumed marker:          1
redraw-submitted marker:        1
network-frame marker:           1
DHCP acknowledgements:          1
input-overflow markers:         0
panic markers:                  0
```

This proves the production republish path could be exercised while EL0 continued
making progress. The forced condition is instrumentation, not natural latency
evidence.

## Natural RX-saturation evidence

A separate test preserved the production deadline while host-forwarded UDP
saturated software-visible virtio-net RX and xHCI input/redraw activity continued.

Recorded result:

```text
EL0 yields:                       38,912
input events consumed:                 16
redraw submissions:                   738
virtio-net frames consumed:        29,234
maximum frames/pass:                   16
network cap exhaustions:             1,827
runtime requeues:                    1,827
natural deadline overruns:               0
maximum pass duration:             385,763 ticks
configured budget:                 625,000 ticks
maximum / budget:                    61.7%
input queue overflow:                    0
panic markers:                           0
```

Evidence boundary:

- consumed frames prove progress only after frames reach software;
- the virtio-net interface did not expose trustworthy device/ring drop counts;
- the deadline was cooperative and could not interrupt an operation already in
  progress;
- the run did not prove indefinite fairness, SMP behavior, or every possible
  redraw/driver duration.

## Provenance

Runtime-service provenance recorded by the original documentation:

- service deadline implementation: PR #60;
- forced-expiry stress: PR #61;
- strict NETWORK routing and natural RX saturation: PR #62;
- original PR #62 merge metadata:
  `7ea3d309047659c8bbe9c601c3d98217bcaafb02`;
- implementation/evidence head:
  `eac4ff990baddbf83406567b4a20e58bcae6600d`;
- final PR #62 head:
  `04f65776d1bbe07545113652342c32f2448bfc7b`;
- original final validation:
  - Verify ArmoniOS run `29896952424`;
  - CI - Tests run `29896952435`.

These identifiers validate the named historical trees. They are not automatically
claims about later documentation-only or feature commits.

## Related correctness fix

A later intermittent VMM fault was traced to IRQs taken in EL1 being treated as
schedulable EL0 process frames. The IRQ-origin gate prevents EL1 frames from
entering process save/preemption or switching TTBR0. Issue #63 records that
separate root-cause investigation and closure.

## Remaining release record

At the time this record was extracted, issue #76 remained open for the final
visible desktop validation, exact v0.2 promotion tree, annotated tag, and release
notes. That manual release-record task is separate from the completed automated
runtime contract.
