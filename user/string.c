unsigned int strlen(const char *s)
{
  int n;

  for(n = 0; s[n]; n++)
    ;
  return n;
}