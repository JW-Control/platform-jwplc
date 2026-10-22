// Compile-only B01 consumer: no network setup or hardware actions in CI.
#include <JWPLC_W5x00_Ethernet.h>

EthernetClient probeClient;
EthernetServer probeServer(502);

void setup() {}

void loop() {
  uint8_t socketStatus = 0xA5;
  const auto clientResult = probeClient.jwplcProbeSocketStatus(socketStatus);
  if (clientResult == jwplc_w5500_candidate::StatusReadOutcome::Read) {
    (void)socketStatus;  // The byte is current only in this branch.
  }

  bool listening = false;
  const auto serverResult = probeServer.jwplcProbeListening(listening);
  if (serverResult == jwplc_w5500_candidate::StatusReadOutcome::Read) {
    (void)listening;  // BUS_BUSY and STALE never provide a new observation.
  }

  // Prior Arduino signatures still compile beside the additive probes.
  (void)probeClient.connected();
  (void)probeClient.status();
  (void)static_cast<bool>(probeServer);
}
