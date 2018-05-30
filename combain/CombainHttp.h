#ifndef COMBAIN_HTTP_H
#define COMBAIN_HTTP_H

void CombainHttpInit(void);
void CombainHttpDeinit(void);
void *CombainHttpThreadFunc(void *context);

#endif // COMBAIN_HTTP_H
