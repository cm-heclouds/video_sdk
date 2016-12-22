#ifndef _ONT_BASE_COMMON_H
#define _ONT_BASE_COMMON_H


#define  ONT_RETURN_IF_NIL(p1,E) \
	if( !p1 ) return E;

#define  ONT_RETURN_IF_NIL2(p1,p2,E) \
	if( !p1 ||!p2 ) return E;

#define  ONT_RETURN_IF_NIL3(p1,p2,p3,E) \
	if( !p1 ||!p2 || !p3 ) return E;

#define ONT_RETURN_ON_ERR(E,ret) \
	if (E)return ret;



#endif /* _ONT_BASE_COMMON_H */
