from setuptools import Extension, setup

ext = Extension(
    name='app',
    sources=['./app.cpp'],
    define_macros=[('PY_SSIZE_T_CLEAN', None)],
    libraries=['gdi32', 'user32', 'opengl32', 'dwmapi'],
)

setup(
    name='app',
    version='0.1.0',
    ext_modules=[ext],
)
