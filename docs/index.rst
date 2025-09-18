fastgltf
========

**fastgltf** is a speed and usability focused glTF 2.0 library written in modern C++20 with minimal dependencies.
It uses SIMD in various areas to decrease the time the application spends parsing and loading glTF data.
By taking advantage of modern C++20 it also provides easy and safe access to the properties and data.
It is also available as a C++20 `named module <https://en.cppreference.com/w/cpp/language/modules>`_.

The library supports the entirety of glTF 2.0 specification, including many extensions.
By default, fastgltf will only do the absolute minimum to work with a glTF model.
However, it brings many additional features to ease working with the data,
including accessor tools, the ability to directly write to mapped GPU buffers, and decomposing transform matrices.

.. note::
   For C++17 compatibility, please use v0.9.x. Later versions require C++20.

Indices and tables
------------------

* :doc:`overview`
* :doc:`tools`
* :doc:`guides`
* :doc:`options`
* :doc:`api`
* :doc:`changelog`


.. toctree::
   :caption: Documentation
   :hidden:
   :maxdepth: 2

   overview
   tools
   guides
   options
   api
   changelog
