.. _external_module_libmpix:

libmpix
#######

Introduction
************

The ``libmpix`` project provides a library for working with image data on microcontrollers. It supports pixel format conversion, debayer, blur, sharpen, color correction, resizing and more.

It pipelines multiple operations together, eliminating intermediate buffers. This allows larger image resolutions to fit in constrained systems without compromising performance.

The library includes a C implementation of classical imaging pipeline operations. Ports or applications can also implement hardware-optimized versions such as ARM Helium SIMD instructions.

libmpix was born from a Zephyr proposal and is now a stand-alone project with tier-1 support for Zephyr.

Features
********

* Simple zero-copy, pipelined engine with low runtime overhead
* Reduces memory overhead (for example processes 1 MB of data with only 5 kB of RAM)
* POSIX support (Linux/BSD/MacOS) and Zephyr support

Upcoming
********

* SIMD acceleration and 2.5D GPU acceleration
* MicroPython and Lua support

Usage with Zephyr
*****************

Add the module in a west manifest or a submanifest and run ``west update``::

   manifest:
     projects:
       - name: libmpix
         url: https://github.com/libmpix/libmpix.git
         revision: main
         path: modules/lib/libmpix

Refer to the ``libmpix`` headers for API details. A brief example is shown below.

.. code-block:: c

   #include <mpix/image.h>

   struct mpix_image img;

   mpix_image_from_buf(&img, buf_in, sizeof(buf_in), MPIX_FORMAT_RGB24);
   mpix_image_kernel(&img, MPIX_KERNEL_DENOISE, 5);
   mpix_image_kernel(&img, MPIX_KERNEL_SHARPEN, 3);
   mpix_image_convert(&img, MPIX_FORMAT_YUYV);
   mpix_image_to_buf(&img, buf_out, sizeof(buf_out));

   return img.err;

Reference
*********

.. _libmpix: https://github.com/libmpix/libmpix
