from distutils.core import setup, Extension

module1 = Extension('spam',
                    sources = ['spammodule.cc'],
                    include_dirs = ['/usr/include/python3.6m'])


setup (name = 'spam',
        version = '1.0',
        description = 'This is a demo package',
        ext_modules = [module1])
