#ifndef __GLEXT2D_H__
#define __GLEXT2D_H__

#define ZOOM_MAX_2D 16.0
#define ZOOM_MIN_2D 1.75
#define ZOOM_START_2D ZOOM_MIN_2D

void glext2d_input_view_parameters(FILE *in);
void glext2d_output_view_parameters(FILE *out);

#endif
