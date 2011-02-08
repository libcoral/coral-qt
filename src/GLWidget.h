#ifndef _GLWIDGET_H_
#define _GLWIDGET_H_

#include <QGLWidget>
#include "GLWidget_Base.h"

namespace qt {

// Forward declaration
class IPainter;

class GLWidget : public QGLWidget, public GLWidget_Base {

	Q_OBJECT

public:
	GLWidget();
	
	virtual ~GLWidget();

	virtual void initializeGL();
	
	virtual void paintGL();

	virtual void resizeGL( int w, int h );

	virtual void setPainter( qt::IPainter* painter );

	virtual IPainter* getPainter();

	virtual const Object& getWidget();

private:
	IPainter* _painter;
};

} // namespace qt;

#endif // _GLWIDGET_H_
