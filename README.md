## Interception Driver Fix

> [**Download installer**](https://github.com/hygorostrowskij/interception-driver-fix/releases/latest)

This application fixes longstanding device reconnection issues for the Interception Driver,
which happens when connecting new devices, or when the computer goes to sleep.

- [Issue: Connecting new devices stops device from working](https://github.com/oblitum/Interception/issues/25)
- [Issue: Interception causes keyboard to become unresponsive if it is unplugged](https://github.com/oblitum/Interception/issues/93)

Additionally, there's an option to lock down permissions for the driver, so that only applications with Administrator privileges have access to it.

This fix should be installed alongside the [Interception Driver](https://github.com/oblitum/Interception). The driver is not included with the fix.

This fix runs entirely in usermode, and does not need to recompile or alter any Interception driver files.


## Configuration

This application can be customized through the `interception-driver-fix.ini` file, which by default is located at `C:/ProgramData/Interception Driver Fix/`.

``` ini
[default]
lockdown=yes  ; Restrict access of the Interception driver only to applications running with Administrator privileges.
```


## Credits

- [Boost Algorithm](https://github.com/boostorg/algorithm)
- [CLI11](https://github.com/CLIUtils/CLI11)
- [fmt](https://github.com/fmtlib/fmt)
- [Material Symbols / Material Icons](https://github.com/google/material-design-icons)
- [phnt](https://github.com/winsiderss/phnt)
- [spdlog](https://github.com/gabime/spdlog)

- Thanks to @oblitum for the incredible Interception Driver.


## License

This project is distributed under a BSD 3-Clause
[license](https://github.com/hygorostrowskij/interception-driver-fix/blob/master/LICENSE).
