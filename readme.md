# secsgml2

Slimmed down version of [secsgml](https://github.com/john-friedman/secsgml) C with SIMD for speed behind a python wrapper. Should be about 8x faster.


```python
from secsgml2 import parse_sgml_content_into_memory

metadata, documents = parse_sgml_content_into_memory(bytes)
```

## Issues

- May not build properly on all machines. If it does not build on your machine, please post a github issue. I am new to OS/Arch specifications.