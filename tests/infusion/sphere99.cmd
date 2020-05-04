@IF NOT EXIST "..\paket\packages\Infusion.Cli.Tools\tools\Infusion.Cli.exe" (
    pushd .
    cd ..\paket
    paket.exe restore
    popd
)

set INFUSION_PATH=%CD%
..\paket\packages\Infusion.Cli.Tools\tools\Infusion.Cli.exe hl --server localhost --serverPort 2593 --proxyPort 60000 --encrypt --clientVersion 3.0.6 --account drzhor --password password --shard Erebor --char DrZhor --script %INFUSION_PATH%\scripts\tests\%1