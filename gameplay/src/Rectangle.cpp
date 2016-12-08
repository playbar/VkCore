#include "Base.h"
#include "Rectangle.h"

namespace vkcore
{

VRectangle::VRectangle()
    : x(0), y(0), width(0), height(0)
{
}

VRectangle::VRectangle(float width, float height) :
    x(0), y(0), width(width), height(height)
{
}

VRectangle::VRectangle(float x, float y, float width, float height) :
    x(x), y(y), width(width), height(height)
{
}

VRectangle::VRectangle(const VRectangle& copy)
{
    set(copy);
}

VRectangle::~VRectangle()
{
}

const VRectangle& VRectangle::empty()
{
    static VRectangle empty;
    return empty;
}

bool VRectangle::isEmpty() const
{
    return (x == 0 && y == 0 && width == 0 && height == 0);
}

void VRectangle::set(const VRectangle& r)
{
    set(r.x, r.y, r.width, r.height);
}

void VRectangle::set(float x, float y, float width, float height)
{
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
}

void VRectangle::setPosition(float x, float y)
{
    this->x = x;
    this->y = y;
}

float VRectangle::left() const
{
    return x;
}

float VRectangle::top() const
{
    return y;
}

float VRectangle::right() const
{
    return x + width;
}

float VRectangle::bottom() const
{
    return y + height;
}

bool VRectangle::contains(float x, float y) const
{
    return (x >= this->x && x <= (this->x + width) && y >= this->y && y <= (this->y + height));
}

bool VRectangle::contains(float x, float y, float width, float height) const
{
    return (contains(x, y) && contains(x + width, y + height));
}

bool VRectangle::contains(const VRectangle& r) const
{
    return contains(r.x, r.y, r.width, r.height);
}

bool VRectangle::intersects(float x, float y, float width, float height) const
{
    float t;
    if ((t = x - this->x) > this->width || -t > width)
        return false;
    if ((t = y - this->y) > this->height || -t > height)
        return false;
    return true;
}

bool VRectangle::intersects(const VRectangle& r) const
{
    return intersects(r.x, r.y, r.width, r.height);
}

bool VRectangle::intersect(const VRectangle& r1, const VRectangle& r2, VRectangle* dst)
{
    GP_ASSERT(dst);

    float xmin = max(r1.x, r2.x);
    float xmax = min(r1.right(), r2.right());
    if (xmax > xmin)
    {
        float ymin = max(r1.y, r2.y);
        float ymax = min(r1.bottom(), r2.bottom());
        if (ymax > ymin)
        {
            dst->set(xmin, ymin, xmax - xmin, ymax - ymin);
            return true;
        }
    }

    dst->set(0, 0, 0, 0);
    return false;
}

void VRectangle::combine(const VRectangle& r1, const VRectangle& r2, VRectangle* dst)
{
    GP_ASSERT(dst);

    dst->x = min(r1.x, r2.x);
    dst->y = min(r1.y, r2.y);
    dst->width = max(r1.x + r1.width, r2.x + r2.width) - dst->x;
    dst->height = max(r1.y + r1.height, r2.y + r2.height) - dst->y;
}

void VRectangle::inflate(float horizontalAmount, float verticalAmount)
{
    x -= horizontalAmount;
    y -= verticalAmount;
    width += horizontalAmount * 2;
    height += verticalAmount * 2;
}

VRectangle& VRectangle::operator = (const VRectangle& r)
{
    x = r.x;
    y = r.y;
    width = r.width;
    height = r.height;
    return *this;
}

bool VRectangle::operator == (const VRectangle& r) const
{
    return (x == r.x && width == r.width && y == r.y && height == r.height);
}

bool VRectangle::operator != (const VRectangle& r) const
{
    return (x != r.x || width != r.width || y != r.y || height != r.height);
}

}
