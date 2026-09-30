#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ $# != 1 ]]; then echo "Usage: $0 /path/to/fork/bin" >&2; exit 2; fi
bin_dir=$(realpath "$1")
feed="$bin_dir/GodotSharp/Tools/nupkgs"
[[ -d "$feed" ]] || { echo "Missing fork NuGet feed: $feed" >&2; exit 1; }
# Use Python XML escaping so paths containing &, quotes, or spaces remain valid.
python3 - "$feed" <<'PY'
import sys
import xml.etree.ElementTree as ET
root = ET.Element('configuration')
sources = ET.SubElement(root, 'packageSources')
ET.SubElement(sources, 'clear')
ET.SubElement(sources, 'add', key='Fork', value=sys.argv[1])
ET.SubElement(sources, 'add', key='nuget', value='https://api.nuget.org/v3/index.json')
ET.ElementTree(root).write('NuGet.Config', encoding='unicode')
PY
export NUGET_PACKAGES="$PWD/.nuget/packages"
dotnet build ForkValidation.csproj -m:1 -p:BuildInParallel=false
