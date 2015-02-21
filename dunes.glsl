// Created by Jeremy Cowles, 2015
// Adapted from Elevated, which was created by inigo quilez - iq/2013
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

// value noise, and its analytical derivatives
vec3 noised( in vec2 x )
{
    // PERFORMANCE: hot spot.
    vec2 p = floor(x);
    vec2 f = fract(x);
    vec2 u = f*f*(3.0-2.0*f);
    #if 0
    float a = 0.1;
    float b = 0.2;
    float c = 0.9;
    float d = 0.5;
    #else
	float a = texture(iChannel0, (p+vec2(0.5,0.5))/256.0, 0).x;
	float b = texture(iChannel0, (p+vec2(1.5,0.5))/256.0, 0).x;
	float c = texture(iChannel0, (p+vec2(0.5,1.5))/256.0, 0).x;
	float d = texture(iChannel0, (p+vec2(1.5,1.5))/256.0, 0).x;
    #endif

	return vec3(a+(b-a)*u.x+(c-a)*u.y+(a-b-c+d)*u.x*u.y,
				6.0*f*(1.0-f)*(vec2(b-a,c-a)+(a-b-c+d)*u.yx));
}

const mat2 m2 = mat2(0.8,-0.6,0.6,0.8);

#define FNs ((xx.y)*.005 + (3.1415/1.)*floor(sin(xx.y*.005)*.5+.5+.5) )
#define FN ((x.y)*.005)
#define FN2 ((x.y+sin(x.x*.015)*10.)*(1.))

// unsigned sin [0,2]
float usin(float x) {
    return sin(x) + 1.0;
}

float udBox( in vec3 p, in vec3 b )
{
	// From iquilezles.com
    return length(max(abs(p)-b,0.0));
}

float desert( in vec2 x)
{
    vec3 n01 = noised(x*.01);
    vec2 xx = x;
    xx.y += 100.*n01.x    ;
        //- iGlobalTime*50.
        //+ n01.z*10.; // Dune evolution, movement
    
    float dunes = sin(FNs) * 100.
                + sin(FN + (x.x*.005+x.y*.0001)) * 30. 
                + 20.*noised(x*.005).x;
    
    float ripples = sin(FN2) * (noised(x*.01).x*(dunes/150.));
    
    dunes += ripples;
    dunes *= sin(FN + (x.x*.01+x.y*.0001))*.25+.75;
    float flt = .001*dunes;
    
    return mix(dunes, flt, smoothstep(-2000., 100., x.y)) +
           mix(flt, dunes, smoothstep(300., 1000., x.y));
}

float desert2( in vec2 x)
{
	return desert(x);
}

float map( in vec3 p )
{
    return p.y - desert(p.xz);
}

float cityScale = 50.;

vec3 cityNoise(in vec3 p)
{
    return noised((floor(p.xz/cityScale)*cityScale)*.03);
}

float city( in vec3 p ) 
{
    vec3 n01 = cityNoise(p);
    
    // city is below sea level
    p.y -= desert((floor(p.xz/cityScale) + .5) * cityScale); 

    //p.y += 40.;
    
    //p.xz = mod(p.xz + n01.xz * 10., scale);
    vec2 m = (.5*n01.xz+.5) * cityScale;
    m = vec2(cityScale,cityScale);
    p.xz = mod(p.xz, m) - 1. * m;
    //p.xz += (.5*n01.xz+.5) * 10.;
    
    //p -= vec3(50.,0.,30.);
    
    vec3 b = vec3(10.+n01.x*20.,13. + n01.y*10.,10. + n01.z*20.);
    // This will fix it:
    b = vec3(14.,13.,15.);
    
	return udBox(p, b);
}

float intersect( in vec3 ro, in vec3 rd, in float tmin, in float tmax, out int prim )
{
    float t = tmin;
    prim = -1;
	for( int i=0; i<120; i++ )
	{
        vec3 p = ro + t*rd;
		float h = map(p);
        if( h<(0.002*t) ) { prim = 0; break; }
        
        float hh = city(p);
        if( hh<(0.002*t) ) { prim = 1; break; }
        
        // prim = -1
        if (t > tmax) break;
        
		t += 0.5*min(h,hh);
	}

	return t;
}

float softShadow(in vec3 ro, in vec3 rd )
{
    // real shadows	
    float res = 1.0;
    float t = .001;
	for( int i=0; i<128; i++ )
	{
	    vec3  p = ro + t*rd;
        float h = map(p);
		res = min( res, 16.0*h/t );
		if( res<0.001 ||p.y>200.0 ) break;

        float hh = city(p);
		res = min( res, hh );
		if( res<0.001 ||p.y>200.0 ) break;
        
       	t += min(hh,h);
	}
	return clamp( res, 0.0, 1.0 );
}

vec3 sandNormal( in vec3 pos, float t )
{
    vec2  eps = vec2( 0.002*t, 0.0 );
    return normalize( vec3( desert2(pos.xz-eps.xy) - desert2(pos.xz+eps.xy),
                            2.0*eps.x,
                            desert2(pos.xz-eps.yx) - desert2(pos.xz+eps.yx) ) );
}

vec3 cityNormal( in vec3 pos, float t )
{
    vec3  eps = vec3( 0.00001*t, 0.00001*t, 0.00 );
    
    return normalize( vec3( city(pos-eps) - city(pos+eps),
                            2.0*eps.x,
                            city(pos-eps) - city(pos+eps) ) );
}

vec3 calcNormal( in vec3 pos, float t, int prim )
{
	return prim == 1 ? cityNormal(pos, t)
                       : sandNormal(pos,t);
}


vec3 camPath( float time )
{
    time -= .5;
	return vec3( time * 1500., sin(time*10.)*30.+30., time * 1000.);
}

float fbm( vec2 p )
{
    float f = 0.0;
    #if 1
    f += 0.5000*texture(iChannel0, p/256.0).x; p = m2*p*2.02;
    f += 0.2500*texture(iChannel0, p/256.0).x; p = m2*p*2.03;
    f += 0.1250*texture(iChannel0, p/256.0).x; p = m2*p*2.01;
    f += 0.0625*texture(iChannel0, p/256.0).x;
    #endif
    return f/0.9375;
}

void main( void )
{
    vec2 xy = -1.0 + 2.0*gl_FragCoord.xy/iResolution.xy;
	vec2 s = xy*vec2(iResolution.x/iResolution.y,1.0);
	
    //float time = iGlobalTime*0.15 + 0.3 + 4.0*iMouse.x/iResolution.x;
    float time = iGlobalTime*0.15 + 0.3 + 4.0/iResolution.x;
	
	vec3 light1 = normalize( vec3(-0.8,0.4,-2.0) );

    // camera position
	vec3 ro = vec3(0); ro = camPath( time );
	vec3 ta = vec3(100,0,0); ta = camPath( time + 3.0 );
	//ro.y = desert( ro.xz ) + 110.0;
    ro.y = max(ro.y, desert(ro.xz)+10.0);
	ta.y = ro.y - 20.0;
	float cr = 0.2*cos(0.1*time);

    // camera ray    
	vec3  cw = normalize(ta-ro);
	vec3  cp = vec3(sin(cr), cos(cr),0.0);
	vec3  cu = normalize( cross(cw,cp) );
	vec3  cv = normalize( cross(cu,cw) );
	vec3  rd = normalize( s.x*cu + s.y*cv + 2.0*cw );
    
    // bounding plane
    float tmin = 10.0;
    float tmax = 2000.0;
    float maxh = 210.0;
    float tp = (maxh-ro.y)/rd.y;
    if( tp>0.0 )
    {
        if( ro.y>maxh ) tmin = max( tmin, tp );
        else            tmax = min( tmax, tp );
    }

	float sundot = clamp(dot(rd,light1),0.0,1.0);
	vec3 col;
    int prim;
    float t = intersect( ro, rd, tmin, tmax, prim );

    vec3 darkBlue = vec3(0.169, 0.31, 0.6);
    vec3 lightBlue = vec3(0.769, 0.843, 0.918);
    
    #if 0
        // Clear day
		vec3 dustYellow = vec3(0.9,0.9,0.6);
    #else
    	// Sand storm -- need to change fog also
    	vec3 dustYellow = vec3(0.9,0.8,0.45);
    #endif
    
    if( t>tmax)
    {
        // sky        
		col = darkBlue*(1.0-0.8*rd.y)*0.9;
        
        // horizon
        //col = mix( col, lightBlue, pow( 1.0-max(rd.y,0.0), 3.0 ) );
        col = mix( col, dustYellow, pow( 1.0-max(rd.y,0.0), 3.0 ) );
	}
	else
	{
        // mountains		
		vec3 pos = ro + t*rd;
        vec3 nor = calcNormal( pos, t, prim );
        vec3 ref = reflect( rd, nor );
        float fre = clamp( 1.0+dot(rd,nor), 0.0, 1.0 );
        
        // rock
		float r = 1.; //texture2D( iChannel0, 7.0*pos.xz/256.0 ).x;
        col = prim == 1 ? vec3(.1,.08,.01) + .5*usin(pos.x*.1 + pos.y*.1)*vec3(.1,.06,.01)
            	: vec3(.24, .1, .0);
		//col = mix( col, 0.20*vec3(0.45,.30,0.15)*(0.50+0.50*r),smoothstep(0.70,0.9,nor.y) );
        //col = mix( col, 0.15*vec3(0.30,.30,0.10)*(0.25+0.75*r),smoothstep(0.95,1.0,nor.y) );
        
		// snow
		float h = smoothstep(55.0,80.0,pos.y + 25.0*fbm(0.01*pos.xz) );
        float e = smoothstep(1.0-0.5*h,1.0-0.1*h,nor.y);
        float o = 0.3 + 0.7*smoothstep(0.0,0.1,nor.x+h*h);
        float s = h*e*o;
        //col = mix( col, 0.29*vec3(0.62,0.65,0.7), smoothstep( 0.1, 0.9, s ) );
		
         // lighting		
        float amb = clamp(0.5+0.5*nor.y,0.0,1.0);
		float dif = clamp( dot( light1, nor ), 0.0, 1.0 );
		float bac = clamp( 0.2 + 0.8*dot( normalize( vec3(-light1.x, 0.0, light1.z ) ), nor ), 0.0, 1.0 );
		float sh = 1.0; if( dif>=0.0001 ) sh = softShadow(pos+light1*20.0,light1);
		
		vec3 lin  = vec3(0.0);
		lin += dif*vec3(7.00,5.00,3.00)*vec3( sh, sh*sh*0.5+0.5*sh, sh*sh*0.8+0.2*sh );
		lin += amb*vec3(0.40,0.60,0.80)*1.2;
        lin += bac*vec3(0.40,0.50,0.60);
		col *= lin;
        
        col += s*0.1*pow(fre,4.0)*vec3(7.0,5.0,3.0)*sh * pow( clamp(dot(light1,ref), 0.0, 1.0),16.0);
        col += s*0.1*pow(fre,4.0)*vec3(0.4,0.5,0.6)*smoothstep(0.0,0.6,ref.y);

		// fog
        #if 1
        	// Clear
            float fo = 1.0-exp(-0.0000000004*t*t*t );
        #else
        	// Sand storm
            float fo = 1.0-exp(-0.000000009*t*t*t );
        #endif
        //vec3 fco = 0.98*mix(dustYellow, lightBlue, t/tmax - .25) + 0.02*vec3(1.0,0.8,0.5)*pow( sundot, 4.0 );
        vec3 fco = 0.98*mix(dustYellow, lightBlue, 0.) + 0.02*vec3(1.0,0.8,0.5)*pow( sundot, 4.0 );
		col = mix( col, fco, fo );

        // sun scatter
		col += 0.3*vec3(1.0,0.8,0.4)*pow( sundot, 8.0 )*(1.0-exp(-0.002*t));
	}

    // gamma
	col = pow(col,vec3(0.4545));

    // vignetting	
	col *= 0.5 + 0.5*pow( (xy.x+1.0)*(xy.y+1.0)*(xy.x-1.0)*(xy.y-1.0), 0.1 );
		
	color = vec4(col,1.0);
}
