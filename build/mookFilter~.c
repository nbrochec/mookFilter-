//
//  mookFilter.cpp
//  max-external
//
//  Created by Nicolas Brochec on 29/07/2022.
//
//  This is free and unencumbered software released into the public domain.
//

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include <math.h>

static t_class *mookFilter_class; // création de la classe


typedef struct _mookFilter
{
    t_pxobject s_ob;
    double s_freq;
    double s_res, s_dynres;
    double s_kfc, s_kf, s_kfcr, s_kacr, s_k2vg;
    double s_lkacr, s_lk2vg;
    double s_ay1, s_ay2, s_ay3, s_ay4, s_amf;
    double s_az1, s_az2, s_az3, s_az4, s_az5;
    double s_resterm;
    double s_fcon;
    double s_rcon;
    double s_sr;
    double s_x1;
    double s_x2;
    
}t_mookFilter;

void mookFilter_float(t_mookFilter *x, double f);
void mookFilter_int(t_mookFilter *x, long n);
void mookFilter_free(t_mookFilter *x);
void mookFilter_clear(t_mookFilter *x);
void mookFilter_calc(t_mookFilter *x);
void mookFilter_dsp64(t_mookFilter *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void mookFilter_perform64(t_mookFilter *x,t_object *dsp64,double **ins,long numins,double **outs,long numouts,long sampleframes, long flags,void  *userparam);
t_max_err mookFilter_attr_setcutoff(t_mookFilter *x, void *attr, long argc, t_atom *argv);
t_max_err mookFilter_attr_setresonance(t_mookFilter *x, void *attr, long argc, t_atom *argv);
void mookFilter_assist(t_mookFilter *x, void *b, long m, long a, char *s);
void *mookFilter_new(t_symbol *s, long argc, t_atom *argv);

C74_EXPORT void ext_main(void *r){
    t_class *c;
    c = class_new("mookFilter~",(method)mookFilter_new, (method)dsp_free,
                  sizeof(t_mookFilter), 0L, A_GIMME, 0);
    class_addmethod(c,(method)mookFilter_dsp64,"dsp64", A_CANT, 0);
    class_addmethod(c,(method)mookFilter_assist, "assist", A_CANT, 0);
    class_addmethod(c,(method)mookFilter_float,"float", A_FLOAT,0);
    class_addmethod(c,(method)mookFilter_int,"int", A_LONG, 0);
    class_addmethod(c,(method)mookFilter_clear, "clear", 0);


    CLASS_ATTR_DOUBLE(c, "cutoff",0, t_mookFilter, s_freq);
    CLASS_ATTR_BASIC(c, "cutoff", 0);
    CLASS_ATTR_LABEL(c, "cutoff", 0, "Cutoff Frequency");
    CLASS_ATTR_ALIAS(c, "cutoff", "freq");
    CLASS_ATTR_ACCESSORS(c, "cutoff", 0, mookFilter_attr_setcutoff);

    CLASS_ATTR_DOUBLE(c, "resonance", 0, t_mookFilter, s_res);
    CLASS_ATTR_BASIC(c, "resonance", 0);
    CLASS_ATTR_LABEL(c, "resonance", 0, "Resonance");
    CLASS_ATTR_ALIAS(c, "resonance", "q");
    CLASS_ATTR_ACCESSORS(c, "resonance", 0, mookFilter_attr_setresonance);
    class_dspinit(c);
    
    class_register(CLASS_BOX,c);
    mookFilter_class=c;
    
}
void mookFilter_free(t_mookFilter *x)
{
    ;
}

void mookFilter_dsp64(t_mookFilter *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags){
   
    x->s_sr = samplerate;
    mookFilter_calc(x);
    x->s_lkacr=x->s_kacr;
    x->s_lk2vg=x->s_k2vg;
    x->s_fcon = count[1];
    x->s_rcon = count[2];
    mookFilter_clear(x);
    object_method(dsp64, gensym("dsp_add64"), x, mookFilter_perform64, 0, NULL);
}

void mookFilter_int(t_mookFilter *x, long n){
    mookFilter_float(x, (double)n);
}

void mookFilter_float(t_mookFilter *x, double f){

    long in = proxy_getinlet((t_object *)x);
    
    if(in==1){
        x->s_freq = f < 1.0 ? 1 : f;
        object_attr_touch((t_object *)x, gensym("cutoff"));
        mookFilter_calc(x);
    }else if(in==2){
        x->s_res = f >= 1.0 ? 1 - 1E-2 : f;
//        x->s_res = f;
        object_attr_touch((t_object *)x, gensym("resonance"));
        mookFilter_calc(x);
        }
}

t_max_err mookFilter_attr_setcutoff(t_mookFilter *x, void *attr, long argc, t_atom *argv){
    double freq = atom_getfloat(argv);
    
    x->s_freq = freq < 1. ? 1 : freq;
    mookFilter_calc(x);
    return 0;
}

t_max_err mookFilter_attr_setresonance(t_mookFilter *x, void *attr, long argc, t_atom *argv){
    double reso = atom_getfloat(argv);
    
    x->s_res = reso >= 1.0 ? 1 - 1E-2 : reso;
//    x->s_res = reso;
    
    mookFilter_calc(x);
    return 0;
}

void mookFilter_perform64(t_mookFilter *x,t_object *dsp64,double **ins,long numins,double **outs,long numouts,long sampleframes,long flags,void  *userparam){
    
    t_double *in1=ins[0];
    t_double *out=outs[0];
    t_double freq = x->s_fcon ? *ins[1] : x->s_freq; // vérifier s'il y a du signal dans les entrées 2 et 3
    t_double res = x->s_rcon ? *ins[2] : x->s_res;
    
    static double ay1;
    static double ay2;
    static double ay3;
    static double ay4;
    
    double amf;
    double az1=x->s_az1;
    double az2=x->s_az2;
    double az3=x->s_az3;
    double az4=x->s_az4;
    double az5=x->s_az5;
    
    double x1 = x->s_x1;
    double x2 = x->s_x2;
    double resterm=x->s_resterm;
    
    double k2vg = x->s_k2vg;
    double kacr = x->s_kacr;
    
    // scale resonance
    if(res >= 1.0){
        res = 1.0 - 1E-2;
    }else if(res < 0.0){
        res = 0.0;
    }
    
    x->s_dynres = res;
    
    if(freq < 1.){
        freq = 1.;
    }
    
    const long v2 = 40000;
    
    // do we need to calculate the coefs ?
    if(freq !=x->s_freq){
        if(freq != x->s_freq){
            x->s_kfc  = x->s_freq / x->s_sr;
            x->s_kf   = x->s_freq / (x->s_sr*2);
            x->s_kfcr = 1.8730*(x->s_kfc*x->s_kfc*x->s_kfc) + 0.4955*(x->s_kfc*x->s_kfc) - 0.6490*x->s_kfc + 0.9988;
            x->s_kacr = kacr = -3.9364*(x->s_kfc*x->s_kfc)    + 1.8409*x->s_kfc       + 0.9968;

            double X  = -2.0 * PI * x->s_kfcr *  x->s_kf;
            double exp_out  = expf(X);

            x->s_k2vg = k2vg = v2*(1-exp_out);
    
        }
        x->s_freq = freq;
        x->s_dynres = res;
    }

    int n;
    
    for(n=0; n<sampleframes; n++){
        x1 = (in1[n] - 4*x->s_dynres*kacr*x->s_amf) / v2;
        float tanh1 = tanhf (x1);
        x2 =  az1 / v2;
        float tanh2 = tanhf (x2);
        ay1 = az1 + k2vg * ( tanh1 - tanh2);
        az1  = ay1;

        ay2  = az2 + k2vg * ( tanh(ay1/v2) - tanh(az2/v2) );
        az2  = ay2;

        ay3  = az3 + k2vg * ( tanh(ay2/v2) - tanh(az3/v2) );
        az3  = ay3;

        ay4  = az4 + k2vg * ( tanh(ay3/v2) - tanh(az4/v2) );
        az4  = ay4;

        // 1/2-sample delay for phase compensation
        x->s_amf  = (ay4+az5)*0.5;
        az5  = ay4;

        // oversampling (repeat same block)
        ay1  = az1 + k2vg * ( tanh( (in1[n] - 4*x->s_dynres*kacr*x->s_amf) / v2) - tanh(az1/v2) );
        az1  = ay1;

        ay2  = az2 + k2vg * ( tanh(ay1/v2) - tanh(az2/v2) );
        az2  = ay2;

        ay3  = az3 + k2vg * ( tanh(ay2/v2) - tanh(az3/v2) );
        az3  = ay3;

        ay4  = az4 + k2vg * ( tanh(ay3/v2) - tanh(az4/v2) );
        az4  = ay4;

        // 1/2-sample delay for phase compensation
        x->s_amf  = (ay4+az5)*0.5;
        az5  = ay4;
    
        out[n]=x->s_amf;
        
        x->s_az1=az1;x->s_az2=az2;x->s_az3=az3;x->s_az4=az4;x->s_az5=az5;
    }
    
}

void mookFilter_clear(t_mookFilter *x){
    
    x->s_az1=x->s_az2=x->s_az4=x->s_az3=x->s_az5=0;
    
}

void mookFilter_assist(t_mookFilter *x, void *b, long m, long a, char *s)
{
    if(m==2){
        sprintf(s, "(signal) Output");
    }else{
        switch (a) {
            case 0: sprintf(s, "(signal) Input"); break;
            case 1: sprintf(s, "(signal/float) Cutoff Frequency"); break;
            case 2: sprintf(s, "(signal/float) Resonance Control (0-1)"); break;
        }
    }
}

void mookFilter_calc(t_mookFilter *x){
    
    const long v2 = 40000;
    
    x->s_kfc  = x->s_freq / x->s_sr;
    x->s_kf   = x->s_freq/ (x->s_sr*2);
    x->s_kfcr = 1.8730*(x->s_kfc*x->s_kfc*x->s_kfc) + 0.4955*(x->s_kfc*x->s_kfc) - 0.6490*x->s_kfc + 0.9988;
    x->s_kacr = -3.9364*(x->s_kfc*x->s_kfc)    + 1.8409*x->s_kfc       + 0.9968;

    double X  = -2.0 * PI * x->s_kfcr *  x->s_kf;
    double exp_out  = expf(X);

    x->s_k2vg = v2*(1-exp_out);
//    x->s_resterm = x->s_kacr * x->s_res;
}

void *mookFilter_new(t_symbol *s, long argc, t_atom *argv){
    t_mookFilter *x = object_alloc(mookFilter_class);
    
    double freq, reso;
    long offset;
    
    offset = attr_args_offset((short)argc, argv);
    dsp_setup((t_pxobject *)x, 3);
    
    if (offset) {
        freq = atom_getlong(argv);
        if ((argv)->a_type == A_LONG){
            object_post((t_object*)x, "arg[%ld] Cutoff Frequency: %ld", 0, atom_getlong(argv));
        }
        if (offset > 1){
            reso = atom_getfloat(argv + 1);
            if ((argv + 1)->a_type == A_FLOAT){
                object_post((t_object*)x, "arg[%ld] Resonance: %f", 1, atom_getfloat(argv+1));
            }
        }else{
            reso = 0;
        }
    }

    x->s_freq = freq < 1. ? 1 : freq;
    x->s_res = reso >= 1.0 ? 1 - 1E-2 : reso;
//    x->s_res= reso;
    
//    if (reso>=1.){
//        reso = 1.f - 1E-20;
//    }else if(reso<0.){
//        reso = 0.f;
//    }
//
//    x->s_res=reso;

    attr_args_process(x, (short)argc, argv);
    
    x->s_sr = sys_getsr();
    mookFilter_calc(x);

    x->s_lkacr=x->s_kacr;
    x->s_lk2vg=x->s_k2vg;
    outlet_new((t_pxobject*)x,"signal");
    return x;
}



