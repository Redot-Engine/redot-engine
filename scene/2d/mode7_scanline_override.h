#pragma once  
  
#include "core/io/resource.h"  
  
class Mode7ScanlineOverride : public Resource {  
    GDCLASS(Mode7ScanlineOverride, Resource);  
  
    // Canonical storage. columns[0] and columns[1] are the 2x2 affine matrix  
    // (rotation/scale/skew); columns[2] is the translation offset.  
    Transform2D transform;  
  
    // Vanishing point around which the matrix is applied.  
    Vector2 pivot = Vector2(0.5f, 0.5f);  
  
    // When true, this entry interpolates smoothly into the next entry in the  
    // scanline override array rather than snapping hard at the boundary.  
    bool lerp = false;  
  
protected:  
    static void _bind_methods();  
  
public:  
    // ── Raw matrix access ────────────────────────────────────────────────────  
    void        set_transform(const Transform2D &p_transform);  
    Transform2D get_transform() const;  
  
    // ── Decomposed convenience properties ────────────────────────────────────  
    // All three read/write through `transform` so the matrix stays canonical.  
    // Exposing all three (rotation + scale + skew) makes the round-trip exact.  
    void   set_rotation(real_t p_radians);  
    real_t get_rotation() const;  
  
    void    set_scale(const Vector2 &p_scale);  
    Vector2 get_scale() const;  
  
    void   set_skew(real_t p_radians);  
    real_t get_skew() const;  
  
    // ── Pivot ────────────────────────────────────────────────────────────────  
    void    set_pivot(const Vector2 &p_pivot);  
    Vector2 get_pivot() const;  
  
    // ── Lerp flag ────────────────────────────────────────────────────────────  
    void set_lerp(bool p_lerp);  
    bool get_lerp() const;  
};