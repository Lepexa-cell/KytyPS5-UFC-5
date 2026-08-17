import re
import sys

filepath = r'm:\apps\KytyPS5-UFC-5\src\loader\runtimeLinker.cpp'

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Replace EXIT with LOGF for the Access violation case
content = content.replace(
    '\t\tEXIT("Access violation: %s [%016" PRIx64 "] %s\\n",',
    '\t\tLOGF("Access violation: %s [%016" PRIx64 "] %s\\n",',
    1
)

# Replace EXIT with LOGF for the Unknown exception case
content = content.replace(
    '\tEXIT("Unknown exception!!! (%08" PRIx32 ")", info->native_code);',
    '\tLOGF("Unknown exception!!! (%08" PRIx32 ")", info->native_code);',
    1
)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)

print("Done")
