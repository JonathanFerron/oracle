# Design Notes (documentationation) about the oracle software

pseudo code for combo bonuses:

s1, s2, s3: species combat cards 1, 2, 3
c1, c2, c3: colours combat cards 1, 2, 3
o1, o2, o3: order combat cards 1, 2, 3

switch deck_drawing_approach

case RANDOM:

  if 2 cards in combat zone:
    if s2=s1 then +10
    else if o2=o1 then +7
    else if c2=c1 then +5

  if 3 cards in combat zone:
    if s2=s1 then
      if s3=s1 then +16
      else if o3=o1 then +14
      else if c3=c1 then +13
      else +10
    else if s3=s1 then
      if o3=o2 then +14
      else if c3=c2 then +13
      else +10
    else if o2=o1 then
      if o3=o1 then +11
      else if c3=c2 then +9
      else +7
    else if o3=o1 then
      if c2=c1 then +9
      else +7
    else if c2=c1 then
      if c3=c1 then +8
      else +5
    else if c3=c1 then = 5
    endif

case MONOCHROME:
  if 2 cards in combat zone:
    if o2=o1 then +7

  if 3 cards in combat zone:
    if o2=o1 then
      if o3=o1 then +12
      else +7
    endif

case CUSTOM:
  if 2 cards in combat zone:
    if s2=s1 then +7
    else if o2=o1 then +4
    
  if 3 cards in combat zone:
    if s2=s1 then
      if s3=s1 then +12
      else if o3=o1 then +9
      else +7
    else if s3=s1 then
      if o3=o2 then +9
      else +7
    else if o2=o1 then
      if o3=o1 then +6
      else +4
    else if o3=o1 then +4
    endif
