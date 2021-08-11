# Model Format

!!! warning

    This project is in-progress, and the format may change.

The model format consists of three sections:

1. Header
1. Main data
1. Vertex data

All values are stored in big endian. Any pointers in C structures are serialized as offsets measured from the start of the same section.

## Header

| Offset | Type      | Description        |
| ------ | --------- | ------------------ |
| 0      | `byte[4]` | Magic              |
| 16     | `uint32`  | Main data offset   |
| 20     | `uint32`  | Main data size     |
| 24     | `uint32`  | Vertex data offset |
| 28     | `uint32`  | Vertex data size   |

- The “magic” identifies this file type, and it consists of the ASCII string “Model” followed by null bytes. The remaining fields locate the other sections in the file.

- The remaining fields describe where the other sections are. Offsets are measured from the beginning of the file.

## Main Data

The main data contains the display lists and various metadata like timing information.

```c
enum {
    MATERIAL_SLOTS = 4,
};
struct model_header {
    Vtx *vertex_data;
    Gfx *display_list[MATERIAL_SLOTS];
    int animation_count;
    unsigned frame_size;
    struct model_animation animation[];
};
```

| Offset | Type         | Description                     |
| ------ | ------------ | ------------------------------- |
| 0      | `uint32`     | Non-animated vertex data offset |
| 4      | `uint32[4]`  | Display list offset             |
| 20     | `uint32`     | Number of animations            |
| 24     | `uint32`     | Frame vertex data size          |
| 28     | `animdata[]` | Animation data                  |

- The non-animated vertex data offset is the relative offset, from the start of the main data section, of the default vertex data. In the current tool, the vertex data is always present. It is not needed for animations, but used instead for non-animated models.

- The display list offsets are the relative offsets, from the start of the main data section, of the display lists for each material in the model. Four materials are supported per model. The display lists refer to vertex data through segment 1. It is assumed that segment 1 points to the beginning of the vertex data for the current animation frame.

- The animation count gives the size of the animation data array.

- The frame vertex data size is the size, in bytes, of the vertex data for a single frame.

### Animation Data

The animation data array consists of items with this structure:

```c
struct model_animation {
    float duration;
    int frame_count;
    struct model_frame *frame;
};
```

| Offset | Type      | Description           |
| ------ | --------- | --------------------- |
| 0      | `float32` | Duration (seconds)    |
| 4      | `uint32`  | Frame count           |
| 8      | `uint32`  | Offset of first frame |

- The offset of the first frame is measured from the beginning of the main data section. It contains an array of frame data.

### Frame Data

The metadata for each frame is stored in frame data structures.

```c
struct model_frame {
    float time;
    float inv_dt;
    unsigned vertex;
};
```

| Offset | Type      | Description                     |
| ------ | --------- | ------------------------------- |
| 0      | `float32` | Duration (seconds)              |
| 4      | `float32` | Inverse of duration (1/seconds) |
| 8      | `uint32`  | Vertex data offset              |

- The vertex data offset is relative to the start of the vertex data section.

## Vertex Data

The vertex data consists of the vertex data for each frame of animation, an array of `Vtx`.

A frame can be selected by mapping the vertex data for that frame into segment 1.
