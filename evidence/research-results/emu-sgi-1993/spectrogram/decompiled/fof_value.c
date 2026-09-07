/* fof_value  entry 0x416df0  size 12 bytes */

double fof_value(double param_1,double param_2)

{
  double in_f0_1;
  double dVar1;
  double dVar2;
  double dVar3;
  double in_stack_00000010;
  
  dVar3 = param_2 * param_2;
  dVar2 = param_1 * param_1 + in_stack_00000010 * in_stack_00000010;
  exp(param_1 * -1.0 * (3.1416 / param_2));
  dVar1 = in_f0_1;
  cos(in_stack_00000010 * (3.1416 / param_2));
  return SQRT((dVar1 * in_f0_1 * 2.0 + in_f0_1 * in_f0_1 + 1.0) /
              ((dVar2 * dVar2 +
               dVar3 * (dVar3 + (param_1 * param_1 - in_stack_00000010 * in_stack_00000010) * 2.0))
              * dVar2)) * (dVar3 / 2.0);
}

