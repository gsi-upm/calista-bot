""" Minimalist Python ctypes wrapper for Unitex

author: Miguel Olivares <miguel@moliware.com>, Javier Herrera

"""
import ctypes
import os
import inspect


# Load library
unitex_lib = '../../Unitex/lib/libunitex.so' if os.name == 'posix' else '../Unitex/lib/unitex.dll'
actual_path= os.path.dirname(os.path.abspath(inspect.getfile(inspect.currentframe())))
unitex = ctypes.cdll.LoadLibrary(actual_path+"/"+unitex_lib)



class Unitex(object):
    """ Create a callable class for an operation"""
    def __getattr__(self, operation):
        """ Return an operation class """
        return UnitexOperation(operation)

class UnitexOperation(object):
    """ Execute any operation of UnitexTool"""
    def __init__(self, operation):
        """ It's allowed every operation of UnitexTool """
        self.operation = operation

    def __call__(self, *params):
        """ Execute operation """
        # Build argv
        params_list = ["UnitexTool", self.operation] + list(params)
        argv_type = ctypes.c_char_p * len(params_list)
        argv = argv_type(*params_list)
        # Build argc
        argc = ctypes.c_int(len(argv))

        if (os.name != 'posix'):
            return unitex.UnitexTool_public_run(argc,argv,None,None)
        else:
            return unitex.main_UnitexTool(argc, argv)  
        

