#include "status.h"
#include "etk_window.h"
#include "etk_widget.h"
#include "etk_platform.h"

static Ret etk_status_paint(EtkWidget *thiz){
	EtkWindowClass *priv;
	Status *status;
	e32 x,y;
	char buf[20];
	priv=(EtkWindowClass*)thiz->subclass;
	status=priv->data[1];
	x=10;y=24;
	sprintf(buf,"����Ũ��:%.1f%%",	status->smoke);
	etk_canvas_draw_string(thiz->canvas,x,y,buf);
	
	sprintf(buf,"�Ž�״̬:%s",status->door_open=0?"��":"��" );
	etk_canvas_draw_string(thiz->canvas,x,y+20,buf);
	sprintf(buf,"����״̬:%s",status->window_open=0?"��":"��" );
	etk_canvas_draw_string(thiz->canvas,x,y+40,buf);

	sprintf(buf,"�ſ�������:%d��",status->door_count);
	etk_canvas_draw_string(thiz->canvas,x,y+60,buf);
	sprintf(buf,"������������:%d��",status->window_count);
	etk_canvas_draw_string(thiz->canvas,x,y+80,buf);
	

	sprintf(buf,"����״̬:%s",status->alarm=0?"����":"�ر�");
	etk_canvas_draw_string(thiz->canvas,x,y+100,buf);
	
	return etk_window_on_paint(thiz);
}

EtkWidget *etk_status_create(e32 x,e32 y,e32 width,e32 height){
	EtkWindowClass *priv;
	Status *status;
	EtkWidget *thiz=etk_create_window(x,y,width,height,ETK_WIDGET_WINDOW);
	thiz->paint=etk_status_paint;
	priv=(EtkWindowClass*)thiz->subclass;
	priv->data[1]=ETK_MALLOC(sizeof(Status));
	status=priv->data[1];
	status->alarm=0;
	status->door_count=0;
	status->door_open=0;
	status->window_count=0;
	status->window_open=0;
	status->smoke=0.0;
	etk_widget_set_text(thiz,"״̬���");
	return thiz;
}
void etk_status_set_alarm(EtkWidget* thiz,e32 value){
	EtkWindowClass *priv;
	Status *status;
	priv=(EtkWindowClass*)thiz->subclass;
	status=priv->data[1];
	status->alarm=value;
}
e32 etk_status_get_door_count(EtkWidget *thiz){
	EtkWindowClass *priv;
	Status *status;
	priv=(EtkWindowClass*)thiz->subclass;
	status=priv->data[1];
	return status->door_count;
}
e32 etk_status_get_window_count(EtkWidget *thiz){
	EtkWindowClass *priv;
	Status *status;
	priv=(EtkWindowClass*)thiz->subclass;
	status=priv->data[1];
	return status->window_count;
}

void etk_status_set_door_count(EtkWidget *thiz,e32 value){
	EtkWindowClass *priv;
	Status *status;
	priv=(EtkWindowClass*)thiz->subclass;
	status=priv->data[1];
	status->door_count=value;
}
void etk_status_set_window_count(EtkWidget *thiz,e32 value){
	EtkWindowClass *priv;
	Status *status;
	priv=(EtkWindowClass*)thiz->subclass;
	status=priv->data[1];
	status->window_count=value;
}