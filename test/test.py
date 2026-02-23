import secsgml2
import secsgml
from time import time
import json
import os

with open('samples/10k.txt','rb') as f:
    content = f.read()

s = time()
metadata, documents = secsgml2.parse_sgml_content_into_memory(content)
print(f"time = {time()-s}")



os.makedirs('samples_output/10k', exist_ok=True)
with open('samples_output/10k/metadata.json', 'w') as f:
    json.dump(metadata, f, ensure_ascii=False, indent=2)

for i, doc_meta in enumerate(metadata.get("documents", [])):
    filename = doc_meta.get("filename") or f"document_{i}.bin"
    out_path = os.path.join('samples_output/10k', filename)
    with open(out_path, 'wb') as f:
        f.write(documents[i])


s = time()
metadata, documents = secsgml.parse_sgml_content_into_memory(content)
print(f"time = {time()-s}")
