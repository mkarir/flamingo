#ifndef __GLEXT3D_H__
#define __GLEXT3D_H__

#define ZOOM_MAX_3D 16.0
#define ZOOM_MIN_3D 1.0
#define ZOOM_START_3D ZOOM_MIN_3D

void glext3d_render_export(guint w, guint h);
void glext3d_input_view_parameters(FILE *in);
void glext3d_output_view_parameters(FILE *out);

#endif
