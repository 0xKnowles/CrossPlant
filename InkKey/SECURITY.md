# InkKey security model

InkKey is a hobbyist hardware token built from consumer e-reader hardware. This
document states plainly what it defends against and what it does not. If your
threat model includes targeted physical attacks, buy a certified hardware token
(YubiKey, Nitrokey…) instead — or in addition.

## Guarantees

**Air gap.** The firmware links no WiFi, Bluetooth, or ESP-NOW code. There is
no network stack in the binary at all — CI inspects every build's symbol table
and fails if `esp_wifi_start`, `esp_bt_controller_enable`, or `esp_now_init`
appear. After one-time USB provisioning, nothing enters or leaves the device.

**Secrets encrypted at rest.** TOTP secrets are stored in internal flash (NVS)
as a single AES-256-GCM blob. The key is derived from the 6-digit PIN with
PBKDF2-HMAC-SHA256 (20 000 iterations, 16-byte random salt, fresh 12-byte
nonce on every write). A wrong PIN and a tampered blob are indistinguishable —
both fail the GCM tag.

**Lock by power-off.** Deep sleep cuts power to RAM. The derived key and the
decrypted account list exist only while the device is awake and unlocked; a
sleeping or dead-battery InkKey holds only ciphertext.

**Serial is gated.** The provisioning console is serviced only while the vault
is unlocked *and* the user has explicitly entered provisioning mode. In every
other state, pending serial bytes are read and discarded. A locked device
plugged into a hostile USB host exposes no command surface.

**Brute-force friction.** Wrong PIN attempts are counted in NVS (surviving
reboot and sleep) and trigger an exponential lockout: 30 s after 3 failures,
doubling to a 15-minute cap. Rebooting restarts the wait, it does not skip it.

**Time integrity.** Codes derive from the DS3231, set once over USB in UTC.
The RTC's oscillator-stop flag is checked; a never-set or power-lost clock is
reported on-screen rather than silently generating wrong codes.

## Non-guarantees — read carefully

**No secure element.** The ESP32-C3 has no hardware key store, and InkKey does
not use flash encryption or secure boot (both would complicate community
flashing of an open firmware). An attacker who physically opens the device can
dump flash, obtain the encrypted vault, and brute-force the PIN **offline** at
whatever speed their hardware allows. A 6-digit PIN has ~10^6 combinations;
PBKDF2 makes each guess cost ~20k hashes, which slows but cannot stop a
determined attacker. **Treat physical loss of the device as eventual compromise
of the secrets on it** and rotate the affected accounts.

**An unlocked device is an open vault.** Anyone holding the device while it is
unlocked sees your codes — that is what it is for. The 3-minute auto-sleep
bounds the window.

**Provisioning is plaintext over USB.** otpauth URIs cross the wire unencrypted
and may persist in your terminal's scrollback. Provision from a machine you
trust, then clear your history.

**No supply-chain defense.** InkKey cannot verify its own integrity. Anyone
with physical access can reflash the device with firmware that leaks secrets at
the next provisioning. Buy your hardware from a source you trust, flash it
yourself from source you have reviewed, and be suspicious of a device that
left your control (the same "evil maid" caveat applies to most open hardware).

**Timing side channels.** The crypto is compact scalar C (constant-time
comparison for tags, but no hardened AES/SHA implementations). The threat this
matters for — an attacker running code on the device while you use it — is
outside the model of a single-purpose, no-radio firmware.

## Reporting

Security issues: open a GitHub issue, or contact the maintainer privately if
the issue is exploitable against provisioned devices in the wild.
