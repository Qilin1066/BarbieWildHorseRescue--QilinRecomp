// XInput (the XPP library: the Xbox's controller ports), replaced natively
// over platform::Input (SDL3's gamepads, and the keyboard on port 1).
//
// From GBTGhoulies--QilinRecomp's, with the functions found by name in this
// game's XDK build (xdk_symbols.inc) and one difference: the engine here
// also asks about memory units, microphones and headphones (its start-up
// reports how many of each are connected). The runtime has none of those
// plugged in, so for a device type that isn't the gamepad's it answers what
// the console does with nothing in the slots: no devices, no changes.
// Opening one stops the runtime.
//
// The engine's input code (bInput, called from 0x115E12 at start-up) calls
// XInitDevices, then XGetDevices and XGetDeviceChanges per type, opens and
// closes ports as they change, reads every open port with XInputGetState,
// and sends rumble with XInputSetState.
#include <algorithm>
#include <array>
#include <string>

#include "host.hpp"
#include "input.hpp"
#include "strf.hpp"

namespace qilin::host {
namespace {

uint32_t g_gamepad = 0;         // XDEVICE_TYPE_GAMEPAD: the library's descriptor (g_DeviceType_Gamepad)
uint32_t g_set_last_error = 0;  // XAPI's SetLastError, lifted
constexpr uint32_t kHandleBase = 0x58490000;  // handles are this plus the port; the game only passes them back
constexpr uint32_t kErrorSuccess = 0, kErrorIoPending = 997, kErrorDeviceNotConnected = 1167;

platform::Input* g_input = nullptr;

struct Port {
    bool open = false;
    uint32_t packet = 0;  // XINPUT_STATE.dwPacketNumber: steps when the state changes
    platform::PadState last;
};
std::array<Port, platform::Input::kPorts> g_ports;
uint32_t g_known = 0;  // the ports the game was last told about (XGetDevices or XGetDeviceChanges)

rt::Mem mem(const rt::Context& c) { return rt::Mem{c.mem, c.mem_size}; }
void st(const rt::Context& c, uint32_t a, uint32_t v) { rt::st32(mem(c), a, v, 0); }

[[noreturn]] void unmodelled(const char* fn, const std::string& what) {
    static std::string why;
    why = strf("%s: %s isn't modelled yet", fn, what.c_str());
    rt::trap(0, why.c_str());
}

void gamepads_only(const char* fn, uint32_t type) {
    if (type != g_gamepad) unmodelled(fn, strf("device type %08X (only gamepads, %08X, are)", type, g_gamepad));
}

uint32_t present() { return (g_input ? g_input->connected_ports() : 0) | (scripted_ports()); }

// The port a handle names, or -1.
int port_of(uint32_t handle) {
    const uint32_t n = handle - kHandleBase;
    return n < g_ports.size() && g_ports[n].open ? int(n) : -1;
}

// VOID XInitDevices(DWORD dwPreallocTypeCount, PXDEVICE_PREALLOC_TYPE PreallocTypes):
// starts the USB stack and reserves handles for the device types listed.
void x_init_devices(rt::Context& c) {
    // (Handles reserved for memory units and voice devices cost nothing here.)
    ret(c, c.eax, 8);
}

// DWORD XGetDevices(PXPP_DEVICE_TYPE DeviceType): a bitmap of the ports with
// a device of that type. What it reports is where XGetDeviceChanges counts from.
void x_get_devices(rt::Context& c) {
    if (arg(c, 0) != g_gamepad) return ret(c, 0, 4);  // a memory unit or a voice device: none plugged in
    g_known = present();
    ret(c, g_known, 4);
}

// BOOL XGetDeviceChanges(PXPP_DEVICE_TYPE DeviceType, PDWORD pdwInsertions,
//     PDWORD pdwRemovals): what was plugged in or removed since the last call.
void x_get_device_changes(rt::Context& c) {
    if (arg(c, 0) != g_gamepad) {  // none came, none went
        st(c, arg(c, 1), 0);
        st(c, arg(c, 2), 0);
        return ret(c, 0, 12);
    }
    const uint32_t now = present();
    const uint32_t insertions = now & ~g_known, removals = g_known & ~now;
    g_known = now;
    st(c, arg(c, 1), insertions);
    st(c, arg(c, 2), removals);
    ret(c, (insertions | removals) ? 1 : 0, 12);
}

// HANDLE XInputOpen(PXPP_DEVICE_TYPE DeviceType, DWORD dwPort, DWORD dwSlot,
//     PXINPUT_POLLING_PARAMETERS pPollingParameters): a handle to the
// controller in a port (dwSlot is for devices plugged into a controller,
// XDEVICE_NO_SLOT here), or NULL with ERROR_DEVICE_NOT_CONNECTED.
void x_input_open(rt::Context& c) {
    gamepads_only("XInputOpen", arg(c, 0));
    const uint32_t port = arg(c, 1), slot = arg(c, 2), polling = arg(c, 3);
    if (slot != 0) unmodelled("XInputOpen", strf("slot %u", slot));
    if (polling) unmodelled("XInputOpen", "polling parameters (the game's own polling rate)");
    if (port >= g_ports.size() || !(present() & (1u << port))) {
        call_guest(c, g_set_last_error, {kErrorDeviceNotConnected});
        return ret(c, 0, 16);
    }
    if (g_ports[port].open) unmodelled("XInputOpen", strf("port %u opened twice", port));
    g_ports[port] = Port{true, 0, {}};
    ret(c, kHandleBase + port, 16);
}

// VOID XInputClose(HANDLE hDevice). Rumble stops with it, as the device's
// state is cleared on the console.
void x_input_close(rt::Context& c) {
    const int port = port_of(arg(c, 0));
    if (port < 0) unmodelled("XInputClose", strf("handle %08X, which isn't open", arg(c, 0)));
    if (g_input) g_input->rumble(uint32_t(port), 0, 0);
    g_ports[size_t(port)].open = false;
    ret(c, c.eax, 4);
}

// DWORD XInputGetState(HANDLE hDevice, PXINPUT_STATE pState): the
// controller's state as XINPUT_STATE { dwPacketNumber; XINPUT_GAMEPAD
// { wButtons; bAnalogButtons[8]; sThumbLX, sThumbLY, sThumbRX, sThumbRY } },
// or ERROR_DEVICE_NOT_CONNECTED once it's been removed.
void x_input_get_state(rt::Context& c) {
    const int port = port_of(arg(c, 0));
    if (port < 0) unmodelled("XInputGetState", strf("handle %08X, which isn't open", arg(c, 0)));
    const platform::PadState s = with_script(uint32_t(port), g_input ? g_input->state(uint32_t(port)) : platform::PadState{});
    if (!s.connected) return ret(c, kErrorDeviceNotConnected, 8);
    Port& p = g_ports[size_t(port)];
    if (s != p.last) {
        ++p.packet;
        p.last = s;
    }
    const rt::Mem m = mem(c);
    const uint32_t out = arg(c, 1);
    rt::st32(m, out, p.packet, 0);
    rt::st16(m, out + 4, s.buttons, 0);
    for (uint32_t i = 0; i < 8; ++i) rt::st8(m, out + 6 + i, s.analog[i], 0);
    for (uint32_t i = 0; i < 4; ++i) rt::st16(m, out + 14 + 2 * i, uint16_t(s.thumbs[i]), 0);
    ret(c, kErrorSuccess, 8);
}

// DWORD XInputSetState(HANDLE hDevice, PXINPUT_FEEDBACK pFeedback): rumble,
// sent to the controller asynchronously. XINPUT_FEEDBACK is a header {
// dwStatus, hEvent, Reserved[58] } and XINPUT_RUMBLE { wLeftMotorSpeed,
// wRightMotorSpeed } at +0x42. The original returns ERROR_IO_PENDING and
// later sets dwStatus and signals hEvent; here the rumble is sent at once,
// so the request is complete as it returns.
void x_input_set_state(rt::Context& c) {
    const int port = port_of(arg(c, 0));
    if (port < 0) unmodelled("XInputSetState", strf("handle %08X, which isn't open", arg(c, 0)));
    const uint32_t fb = arg(c, 1);
    if (rt::ld32(mem(c), fb + 4, 0)) unmodelled("XInputSetState", "a completion event (hEvent)");
    if (g_input) g_input->rumble(uint32_t(port), rt::ld16(mem(c), fb + 0x42, 0), rt::ld16(mem(c), fb + 0x44, 0));
    st(c, fb, kErrorSuccess);
    ret(c, kErrorIoPending, 8);
}

}  // namespace

void register_xinput(platform::Input* input) {
    g_input = input;
    auto symbol = [](const char* name) {
        const XdkSymbol* s = xdk_symbol(name);
        if (!s) {
            static std::string why;
            why = strf("the controller library's %s isn't in xdk_symbols.inc", name);
            rt::trap(0, why.c_str());
        }
        return s->address;
    };
    g_gamepad = symbol("g_DeviceType_Gamepad");
    g_set_last_error = symbol("SetLastError");
    register_hle(0x20421F, "XInitDevices", x_init_devices);  // the library's jump to it, which the engine calls
    register_hle(symbol("XInitDevices"), "XInitDevices", x_init_devices);
    register_hle(symbol("XGetDevices"), "XGetDevices", x_get_devices);
    register_hle(symbol("XGetDeviceChanges"), "XGetDeviceChanges", x_get_device_changes);
    register_hle(symbol("XInputOpen"), "XInputOpen", x_input_open);
    register_hle(symbol("XInputClose"), "XInputClose", x_input_close);
    register_hle(symbol("XInputGetState"), "XInputGetState", x_input_get_state);
    register_hle(symbol("XInputSetState"), "XInputSetState", x_input_set_state);
}

}  // namespace qilin::host
