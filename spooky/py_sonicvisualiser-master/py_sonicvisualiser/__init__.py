
from ._version import get_versions
__version__ = get_versions()['version']
del get_versions

from py_sonicvisualiser.SVEnv import SVEnv
