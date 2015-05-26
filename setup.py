from distutils.core import setup, Extension

module1 = Extension('seeq',
                    define_macros = [('MAJOR_VERSION', '1'),
                                     ('MINOR_VERSION', '0')],
                    include_dirs = ['src/'],
                    sources = ['src/libseeq.c','src/seeqmodule.c'],
                    extra_compile_args = ['-std=c99'])

setup (name = 'seeq',
       version = '1.1',
       description = 'Seeq library for Python',
       author = 'Eduard V. Zorita',
       author_email = 'eduardvalera@gmail.com',
       url = 'https://docs.python.org/extending/building',
       long_description = 'Seeq library for Python\n',
       ext_modules = [module1])
