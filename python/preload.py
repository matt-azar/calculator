"""Math helpers for the Calculator IPython session.

Imported into the interactive namespace so `sin(pi/2)` works with no imports.
"""

from math import (
    acos,
    asin,
    atan,
    atan2,
    ceil,
    cos,
    cosh,
    degrees,
    e,
    exp,
    fabs,
    factorial,
    floor,
    gcd,
    inf,
    lcm,
    log,
    log2,
    log10,
    nan,
    pi,
    pow,
    radians,
    sin,
    sinh,
    sqrt,
    tan,
    tanh,
    tau,
)
from cmath import phase, polar, rect
from cmath import exp as cexp
from cmath import log as clog
from cmath import sqrt as csqrt
from statistics import mean, median, pstdev, stdev, variance

print("Math helpers loaded (math / cmath / statistics). Use _ for last result.")
