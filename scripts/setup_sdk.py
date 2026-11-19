"""Freeze the reviewed SDK source snapshot. No pip, wheel or private Python runtime."""
from cymcp_dependency import resolve
if __name__=='__main__':print(resolve())
