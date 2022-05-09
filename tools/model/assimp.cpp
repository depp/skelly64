// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include <assimp/matrix4x4.h>
#include <assimp/matrix4x4.inl>

template aiMatrix4x4t<float>::aiMatrix4x4t();
template float *aiMatrix4x4t<float>::operator[](unsigned);
