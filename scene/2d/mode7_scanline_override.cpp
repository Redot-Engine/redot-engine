#include "mode7_scanline_override.h"  
  
void Mode7ScanlineOverride::set_transform(const Transform2D &p_transform) {  
    transform = p_transform;  
    emit_changed();  
}  
Transform2D Mode7ScanlineOverride::get_transform() const { return transform; }  
  
void Mode7ScanlineOverride::set_rotation(real_t p_radians) {  
    transform.set_rotation_scale_and_skew(p_radians, transform.get_scale(), transform.get_skew());  
    emit_changed();  
}  
real_t Mode7ScanlineOverride::get_rotation() const { return transform.get_rotation(); }  
  
void Mode7ScanlineOverride::set_scale(const Vector2 &p_scale) {  
    // Invert so scale > 1 = zoom in (image appears larger), matching  
    // standard 2D convention rather than raw UV-space convention.  
    Vector2 inv(1.0f / p_scale.x, 1.0f / p_scale.y);  
    transform.set_scale(inv);  
    emit_changed();  
}  

Vector2 Mode7ScanlineOverride::get_scale() const {  
    Vector2 s = transform.get_scale();  
    return Vector2(1.0f / s.x, 1.0f / s.y);  
}
  
void Mode7ScanlineOverride::set_skew(real_t p_radians) {  
    transform.set_rotation_scale_and_skew(transform.get_rotation(), transform.get_scale(), p_radians);  
    emit_changed();  
}  
real_t Mode7ScanlineOverride::get_skew() const { return transform.get_skew(); }  
  
void Mode7ScanlineOverride::set_pivot(const Vector2 &p_pivot) { pivot = p_pivot; emit_changed(); }  
Vector2 Mode7ScanlineOverride::get_pivot() const { return pivot; }  
  
void Mode7ScanlineOverride::set_lerp(bool p_lerp) { lerp = p_lerp; emit_changed(); }  
bool Mode7ScanlineOverride::get_lerp() const { return lerp; }  
  
void Mode7ScanlineOverride::_bind_methods() {  
    ClassDB::bind_method(D_METHOD("set_transform", "transform"), &Mode7ScanlineOverride::set_transform);  
    ClassDB::bind_method(D_METHOD("get_transform"),              &Mode7ScanlineOverride::get_transform);  
    ClassDB::bind_method(D_METHOD("set_rotation", "radians"),    &Mode7ScanlineOverride::set_rotation);  
    ClassDB::bind_method(D_METHOD("get_rotation"),               &Mode7ScanlineOverride::get_rotation);  
    ClassDB::bind_method(D_METHOD("set_scale", "scale"),         &Mode7ScanlineOverride::set_scale);  
    ClassDB::bind_method(D_METHOD("get_scale"),                  &Mode7ScanlineOverride::get_scale);  
    ClassDB::bind_method(D_METHOD("set_skew", "radians"),        &Mode7ScanlineOverride::set_skew);  
    ClassDB::bind_method(D_METHOD("get_skew"),                   &Mode7ScanlineOverride::get_skew);  
    ClassDB::bind_method(D_METHOD("set_pivot", "pivot"),         &Mode7ScanlineOverride::set_pivot);  
    ClassDB::bind_method(D_METHOD("get_pivot"),                  &Mode7ScanlineOverride::get_pivot);  
    ClassDB::bind_method(D_METHOD("set_lerp", "lerp"),           &Mode7ScanlineOverride::set_lerp);  
    ClassDB::bind_method(D_METHOD("get_lerp"),                   &Mode7ScanlineOverride::get_lerp);  
  
    ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "transform"),                        "set_transform", "get_transform");  
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rotation", PROPERTY_HINT_RANGE,  
            "-360,360,0.1,radians_as_degrees"),                                          "set_rotation",  "get_rotation");  
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scale"),                                "set_scale",     "get_scale");  
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "skew", PROPERTY_HINT_RANGE,  
            "-89.9,89.9,0.1,radians_as_degrees"),                                        "set_skew",      "get_skew");  
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "pivot"),                                "set_pivot",     "get_pivot");  
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lerp"),                                    "set_lerp",      "get_lerp");  
}