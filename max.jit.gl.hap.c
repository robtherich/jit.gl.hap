// max.jit.gl.hap.c
// sample GL group object which draws a simple quadrilateral. no matrixoutput.
// Copyright 2002-2005 - Cycling '74

#include "jit.common.h"
#include "jit.gl.h"


typedef struct _max_jit_gl_hap 
{
	t_object		ob;
	void			*obex;
	void			*texout;
} t_max_jit_gl_hap;

t_jit_err jit_gl_hap_init(void); 

void max_jit_gl_hap_assist(t_max_jit_gl_hap *x, void *b, long io, long index, char *s);
void max_jit_gl_hap_notify(t_max_jit_gl_hap *x, t_symbol *s, t_symbol *msg, void *ob, void *data);

void max_jit_gl_hap_bang(t_max_jit_gl_hap *x);
void max_jit_gl_hap_draw(t_max_jit_gl_hap *x, t_symbol *s, long argc, t_atom *argv);

void *max_jit_gl_hap_new(t_symbol *s, long argc, t_atom *argv);
void max_jit_gl_hap_free(t_max_jit_gl_hap *x);

t_class *max_jit_gl_hap_class;

t_symbol *ps_jit_gl_texture;
t_symbol *ps_draw;
t_symbol *ps_out_name;
t_symbol *ps_typedmess;

int C74_EXPORT main(void)
{	
	t_class *maxclass, *jitclass;
	
	jit_gl_hap_init();
	

	maxclass = class_new("jit.gl.hap", (method)max_jit_gl_hap_new, (method)max_jit_gl_hap_free, sizeof(t_max_jit_gl_hap), NULL, A_GIMME, 0);
	max_jit_class_obex_setup(maxclass, calcoffset(t_max_jit_gl_hap, obex));
	jitclass = jit_class_findbyname(gensym("jit_gl_hap"));
    max_jit_class_wrap_standard(maxclass, jitclass, 0);
    
	// override default ob3d bang/draw methods
	max_jit_class_addmethod_defer_low(maxclass, (method)max_jit_gl_hap_bang, "bang");
	max_jit_class_addmethod_defer_low(maxclass, (method)max_jit_gl_hap_draw, "draw");
				
    class_addmethod(maxclass, (method)max_jit_gl_hap_assist, "assist", A_CANT, 0);

	// add methods for 3d drawing
    max_jit_class_ob3d_wrap(maxclass);
	
	// override the default ob3d notify with our own. must come after max_jit_class_ob3d_wrap
	class_addmethod(maxclass, (method)max_jit_gl_hap_notify, "notify", A_CANT, 0);
		
	// register our class with max
	class_register(CLASS_BOX, maxclass);
	max_jit_gl_hap_class = maxclass;
	
	ps_jit_gl_texture = gensym("jit_gl_texture");
	ps_draw = gensym("draw");
	ps_out_name = gensym("out_name");
	ps_typedmess = gensym("typedmess");
	
	return 0;
}

void max_jit_gl_hap_assist(t_max_jit_gl_hap *x, void *b, long io, long index, char *s)
{
	if (io == 1) {	//input
		if(index == 0) 
			sprintf(s,"messages to this 3d object");
		//else
		//	sprintf(s,"texture or matrix input");
	}
	else {			//output
		if(index == 0) 
			sprintf(s,"texture output");
		else 
			sprintf(s, "dumpout");
	}
}

void max_jit_gl_hap_notify(t_max_jit_gl_hap *x, t_symbol *s, t_symbol *msg, void *ob, void *data)
{
	t_atom a;
	if (msg==ps_draw) {
		jit_atom_setsym(&a, jit_attr_getsym(max_jit_obex_jitob_get(x), ps_out_name));
		outlet_anything(x->texout,ps_jit_gl_texture,1,&a);
	}
	else if (msg == ps_typedmess && data) {
		long ac = 0;
		t_atom *av = NULL;

		object_getvalueof(data, &ac, &av);	
		if (ac && av) {
			msg = jit_atom_getsym(av);
			/*if (msg == ps_framereport) {
				double d=(long)data;
				t_atom a;

				d = jit_atom_getfloat(av + 1);
				jit_atom_setfloat(&a, d*0.001); //convert micro to milliseconds
				max_jit_obex_dumpout(x, ps_framecalc, 1, &a);
			}*/
			//else {
				max_jit_obex_dumpout(x, msg, ac - 1, av + 1);
			//}
			freebytes(av, sizeof(t_atom) * ac);
		}
	}	
}

void max_jit_gl_hap_bang(t_max_jit_gl_hap *x)
{
	typedmess((t_object *)x,ps_draw,0,NULL);
}

void max_jit_gl_hap_draw(t_max_jit_gl_hap *x, t_symbol *s, long argc, t_atom *argv)
{
	t_atom a;
	// get the jitter object
	t_jit_object *jitob = (t_jit_object*)max_jit_obex_jitob_get(x);
	
	// call the jitter object's draw method
	jit_object_method(jitob,s,s,argc,argv);

	// query the texture name and send out the texture output 
	jit_atom_setsym(&a,jit_attr_getsym(jitob,ps_out_name));
	outlet_anything(x->texout,ps_jit_gl_texture,1,&a);
}

void max_jit_gl_hap_free(t_max_jit_gl_hap *x)
{
	void * jitob = max_jit_obex_jitob_get(x);
	
	jit_object_detach(jit_attr_getsym(jitob, _jit_sym_name),x);
	
	// lookup our internal Jitter object instance and free
	jit_object_free(jitob);
	
	// free resources associated with our obex entry
	max_jit_object_free(x);
}

void *max_jit_gl_hap_new(t_symbol *s, long argc, t_atom *argv)
{
	t_max_jit_gl_hap *x;
	void *jit_ob;
	long attrstart;
	t_symbol *dest_name_sym = _jit_sym_nothing;

	if ((x = (t_max_jit_gl_hap *)max_jit_object_alloc(max_jit_gl_hap_class, gensym("jit_gl_hap"))))
	{
		// get first normal arg, the destination name
		attrstart = max_jit_attr_args_offset(argc,argv);
		if (attrstart&&argv) 
		{
			jit_atom_arg_getsym(&dest_name_sym, 0, attrstart, argv);
		}

		// instantiate Jitter object with dest_name arg
		if ((jit_ob = jit_object_new(gensym("jit_gl_hap"), dest_name_sym))) 
		{
			// set internal jitter object instance
			max_jit_obex_jitob_set(x, jit_ob);
			
			// add a general purpose outlet (rightmost)
			max_jit_obex_dumpout_set(x, outlet_new(x,NULL));
			
			// process attribute arguments 
			max_jit_attr_args(x, argc, argv);
			
			x->texout = outlet_new(x, "jit_gl_texture");
			
			//attach max object(client) with jit object(server)
			jit_object_attach(jit_attr_getsym(jit_ob, _jit_sym_name),x);
			
			// attach the jit object's ob3d to a new outlet 	
			// this outlet is used in matrixoutput mode
			//max_jit_ob3d_attach(x, jit_ob, outlet_new(x, "jit_matrix"));
		} 
		else 
		{
			error("jit.gl.hap: could not allocate object");
			freeobject((t_object *)x);
			x = NULL;
		}
	}
	return (x);
}


