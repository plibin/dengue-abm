delete from parameters where vac = 0 and catchup = 1;
delete from parameters where vac = 0 and target != 9;
delete from parameters where vac = 0 and (vac_waning != 0 or vac_boosting != 0);
delete from parameters where vac_waning = 0 and vac_boosting != 0;
delete from jobs where serial not in (select serial from parameters);
delete from metrics where serial not in (select serial from parameters);
