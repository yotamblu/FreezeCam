; FreezeCam Pro - Custom NSIS Installer Hooks
; Registers virtual camera DLL, disables Frame Server, and handles uninstall cleanup

!include "LogicLib.nsh"

;-----------------------------------------------
; INSTALL: runs after files are extracted
;-----------------------------------------------
!macro customInstall
  ; --- Register the DirectShow virtual camera DLL ---
  DetailPrint "Registering FreezeCam Pro Virtual Camera..."
  nsExec::ExecToLog 'regsvr32 /s "$INSTDIR\resources\native\FreezeCamVirtualCamera.dll"'
  Pop $0
  ${If} $0 != "0"
    DetailPrint "WARNING: Virtual camera registration returned code $0"
    DetailPrint "Will retry with elevated regsvr32..."
    nsExec::ExecToLog '"$SYSDIR\regsvr32.exe" /s "$INSTDIR\resources\native\FreezeCamVirtualCamera.dll"'
    Pop $0
  ${EndIf}
  DetailPrint "Virtual camera registration complete (exit code: $0)"

  ; --- Disable Windows Camera Frame Server ---
  ; Required for DirectShow virtual cameras to work in Zoom on Windows 10/11
  DetailPrint "Disabling Windows Camera Frame Server..."
  nsExec::ExecToLog 'reg add "HKLM\SOFTWARE\Microsoft\Windows Media Foundation\Platform" /v EnableFrameServerMode /t REG_DWORD /d 0 /f'
  Pop $0
  DetailPrint "Frame Server 64-bit key set (exit code: $0)"

  nsExec::ExecToLog 'reg add "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows Media Foundation\Platform" /v EnableFrameServerMode /t REG_DWORD /d 0 /f'
  Pop $0
  DetailPrint "Frame Server 32-bit key set (exit code: $0)"

  DetailPrint "FreezeCam Pro installation complete!"
!macroend

;-----------------------------------------------
; UNINSTALL: runs before files are removed
;-----------------------------------------------
!macro customUnInstall
  ; --- Unregister the DirectShow virtual camera DLL ---
  DetailPrint "Unregistering FreezeCam Pro Virtual Camera..."
  nsExec::ExecToLog 'regsvr32 /u /s "$INSTDIR\resources\native\FreezeCamVirtualCamera.dll"'
  Pop $0
  ${If} $0 != "0"
    nsExec::ExecToLog '"$SYSDIR\regsvr32.exe" /u /s "$INSTDIR\resources\native\FreezeCamVirtualCamera.dll"'
    Pop $0
  ${EndIf}
  DetailPrint "Virtual camera unregistration complete (exit code: $0)"

  ; --- Re-enable Frame Server (restore default Windows behavior) ---
  DetailPrint "Re-enabling Windows Camera Frame Server..."
  nsExec::ExecToLog 'reg delete "HKLM\SOFTWARE\Microsoft\Windows Media Foundation\Platform" /v EnableFrameServerMode /f'
  Pop $0
  nsExec::ExecToLog 'reg delete "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows Media Foundation\Platform" /v EnableFrameServerMode /f'
  Pop $0
  DetailPrint "Frame Server restored to default"

  DetailPrint "FreezeCam Pro uninstallation complete!"
!macroend
