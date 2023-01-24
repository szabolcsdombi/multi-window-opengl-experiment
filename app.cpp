#include <Python.h>

#define WINVER 0x0601
#include <Windows.h>
#include <Dwmapi.h>
#include <GL/GL.h>

#define WGL_CONTEXT_PROFILE_MASK 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT 0x0001
#define WGL_CONTEXT_MAJOR_VERSION 0x2091
#define WGL_CONTEXT_MINOR_VERSION 0x2092
#define WGL_CONTEXT_FLAGS 0x2094
#define WGL_CONTEXT_DEBUG_BIT 0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT 0x0002
#define WGL_CONTEXT_FLAG_NO_ERROR_BIT 0x0008

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hdc, HGLRC hrc, const int * attrib_list);
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

HWND base_hwnd;
HDC base_hdc;
HGLRC base_hrc;

struct Window {
    PyObject_HEAD

    HWND hwnd;
    HDC hdc;
    HGLRC hrc;

    int width;
    int height;
};

PyTypeObject * Window_type;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam) {
    switch (umsg) {
        case WM_CLOSE: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, umsg, wparam, lparam);
}

Window * meth_window(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"position", "size", NULL};

    int x;
    int y;
    int width;
    int height;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "(ii)(ii)", keywords, &x, &y, &width, &height)) {
        return NULL;
    }

    HINSTANCE hinst = GetModuleHandle(NULL);
    HCURSOR hcursor = (HCURSOR)LoadCursor(NULL, IDC_ARROW);
    WNDCLASS wnd_class = {CS_OWNDC, WindowProc, 0, 0, hinst, NULL, hcursor, NULL, NULL, "mywindow"};
    RegisterClass(&wnd_class);

    int style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, style, false);

    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;

    HWND hwnd = CreateWindow("mywindow", "Window", style, x, y, w, h, NULL, NULL, hinst, NULL);
    if (!hwnd) {
        PyErr_BadInternalCall();
        return NULL;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        PyErr_BadInternalCall();
        return NULL;
    }

    DWORD pfd_flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED;
    PIXELFORMATDESCRIPTOR pfd = {sizeof(PIXELFORMATDESCRIPTOR), 1, pfd_flags, 0, 32};

    int pixelformat = ChoosePixelFormat(hdc, &pfd);
    if (!pixelformat) {
        PyErr_BadInternalCall();
        return NULL;
    }

    if (!SetPixelFormat(hdc, pixelformat, &pfd)) {
        PyErr_BadInternalCall();
        return NULL;
    };

    int attribs[] = {
        WGL_CONTEXT_FLAGS, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT,
        WGL_CONTEXT_PROFILE_MASK, WGL_CONTEXT_CORE_PROFILE_BIT,
        WGL_CONTEXT_MAJOR_VERSION, 4,
        WGL_CONTEXT_MINOR_VERSION, 5,
        0, 0,
    };

    HGLRC hrc = wglCreateContextAttribsARB(hdc, base_hrc, attribs);
    if (!hrc) {
        PyErr_BadInternalCall();
        return NULL;
    }

    Window * res = PyObject_New(Window, Window_type);
    res->hwnd = hwnd;
    res->hdc = hdc;
    res->hrc = hrc;
    res->width = width;
    res->height = height;
    return res;
}

PyObject * meth_update(PyObject * self) {
    SwapBuffers(base_hdc);
    DwmFlush();

    MSG msg = {};
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            Py_RETURN_FALSE;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Py_RETURN_TRUE;
}

PyObject * Window_meth_enter(Window * self) {
    wglMakeCurrent(self->hdc, self->hrc);
    Py_RETURN_NONE;
}

PyObject * Window_meth_exit(Window * self) {
    wglMakeCurrent(base_hdc, base_hrc);
    Py_RETURN_NONE;
}

void default_dealloc(PyObject * self) {
    Py_TYPE(self)->tp_free(self);
}

PyMethodDef Window_methods[] = {
    {"__enter__", (PyCFunction)Window_meth_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)Window_meth_exit, METH_VARARGS},
    {},
};

PyType_Slot Window_slots[] = {
    {Py_tp_methods, Window_methods},
    {Py_tp_dealloc, default_dealloc},
    {},
};

PyType_Spec Window_spec = {"app.Window", sizeof(Window), 0, Py_TPFLAGS_DEFAULT, Window_slots};

PyMethodDef module_methods[] = {
    {"window", (PyCFunction)meth_window, METH_VARARGS | METH_KEYWORDS},
    {"update", (PyCFunction)meth_update, METH_NOARGS},
    {},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "app", NULL, -1, module_methods};

extern "C" PyObject * PyInit_app() {
    HINSTANCE hinst = GetModuleHandle(NULL);
    HCURSOR hcursor = (HCURSOR)LoadCursor(NULL, IDC_ARROW);
    WNDCLASS wnd_class = {CS_OWNDC, WindowProc, 0, 0, hinst, NULL, hcursor, NULL, NULL, "mywindow"};
    RegisterClass(&wnd_class);

    base_hwnd = CreateWindow("mywindow", NULL, 0, 0, 0, 0, 0, NULL, NULL, hinst, NULL);
    if (!base_hwnd) {
        PyErr_BadInternalCall();
        return NULL;
    }

    base_hdc = GetDC(base_hwnd);
    if (!base_hdc) {
        PyErr_BadInternalCall();
        return NULL;
    }

    DWORD pfd_flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED;
    PIXELFORMATDESCRIPTOR pfd = {sizeof(PIXELFORMATDESCRIPTOR), 1, pfd_flags, 0, 32};

    int pixelformat = ChoosePixelFormat(base_hdc, &pfd);
    if (!pixelformat) {
        PyErr_BadInternalCall();
        return NULL;
    }

    if (!SetPixelFormat(base_hdc, pixelformat, &pfd)) {
        PyErr_BadInternalCall();
        return NULL;
    }

    HGLRC loader_hglrc = wglCreateContext(base_hdc);
    if (!loader_hglrc) {
        PyErr_BadInternalCall();
        return NULL;
    }

    if (!wglMakeCurrent(base_hdc, loader_hglrc)) {
        PyErr_BadInternalCall();
        return NULL;
    }

    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    if (!wglCreateContextAttribsARB) {
        PyErr_BadInternalCall();
        return NULL;
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(loader_hglrc);

    int attribs[] = {
        WGL_CONTEXT_FLAGS, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT,
        WGL_CONTEXT_PROFILE_MASK, WGL_CONTEXT_CORE_PROFILE_BIT,
        WGL_CONTEXT_MAJOR_VERSION, 4,
        WGL_CONTEXT_MINOR_VERSION, 5,
        0, 0,
    };

    base_hrc = wglCreateContextAttribsARB(base_hdc, NULL, attribs);
    if (!base_hrc) {
        PyErr_BadInternalCall();
        return NULL;
    }

    wglMakeCurrent(base_hdc, base_hrc);

    PyObject * module = PyModule_Create(&module_def);
    Window_type = (PyTypeObject *)PyType_FromSpec(&Window_spec);
    return module;
}
