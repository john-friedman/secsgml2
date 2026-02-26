# secsgml2

Slimmed down version of [secsgml](https://github.com/john-friedman/secsgml) C with SIMD for speed behind a python wrapper. Should be about 8x faster.

> Note that performance may be hard to check, as parsing is much faster than write.

Parse SEC SGML
```python
from secsgml2 import parse_sgml_content_into_memory

# dictionary in string form, documents in bytes
metadata, documents = parse_sgml_content_into_memory(bytes)
```

Parse uudecoded bytes where begin and end have already been removed.
```
from secsgml2 import decode_uuencoded_content
decode_uuencoded_content(bytes)
```

## Issues

- May not build properly on all machines. If it does not build on your machine, please post a github issue. I am new to OS/Arch specifications.
