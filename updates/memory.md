# Memory / Resource Leak Fixes

## `src/main.cpp`

### 1. `bpf_program` never freed

| | |
|:---|:---|
| **Location** | `main()` — after `pcap_setfilter()` |
| **Issue** | `pcap_compile()` allocates memory for the compiled BPF filter, but `pcap_freecode()` was never called. |
| **Fix** | Added `pcap_freecode(&fcode);` immediately after `pcap_setfilter()` succeeds. |

```cpp
if (pcap_setfilter(g_adhandle, &fcode) < 0) { /* ... */ }
pcap_freecode(&fcode);   // <-- added
```

### 2. pcap handle leaked on `pcap_compile` error

| | |
|:---|:---|
| **Location** | `main()` — `pcap_compile` failure branch |
| **Issue** | If compilation failed after `pcap_open()` succeeded, `g_adhandle` was left open. |
| **Fix** | Added `pcap_close(g_adhandle);` before bailing out. |

```cpp
if (pcap_compile(g_adhandle, &fcode, ...) < 0) {
    pcap_close(g_adhandle);     // <-- added
    pcap_freealldevs(alldevs);
    return 1;
}
```

### 3. pcap handle + `bpf_program` leaked on `pcap_setfilter` error

| | |
|:---|:---|
| **Location** | `main()` — `pcap_setfilter` failure branch |
| **Issue** | Both the compiled filter and the adapter handle were leaked on failure. |
| **Fix** | Free the filter and close the handle before exiting. |

```cpp
if (pcap_setfilter(g_adhandle, &fcode) < 0) {
    pcap_freecode(&fcode);      // <-- added
    pcap_close(g_adhandle);     // <-- added
    pcap_freealldevs(alldevs);
    return 1;
}
```

---

## Files Checked — No Leaks Found

| File | Notes |
|:---|:---|
| `src/gui.hpp` | GDI objects validated and deleted after modal loop. |
| `src/device_select.hpp` | GDI objects cleaned up; WM_DRAWITEM creates/deletes per-draw (suboptimal but not leaking). |
| `src/dashboard.hpp` | Memory DC / bitmap / brush properly paired; fonts & brushes deleted after loop. |
| `src/gui_common.hpp` | `draw_rounded_rect` saves/restores old brush/pen and deletes temporaries. |
| `src/heuristic.hpp` | No raw allocations; standard containers manage memory automatically. |
| `src/tls_parser.hpp` | Pure packet parsing — no heap usage. |
| `src/config.hpp` | File I/O uses RAII streams. |
