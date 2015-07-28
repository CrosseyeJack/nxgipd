/* process.c
 *
 * Copyright (C) 2011 Timo Kokkonen.
 * All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "nx-584.h"
#include "nxgipd.h"



#define LOG_STATUS_CHANGE(o,n,chg,t,f)   { if (o!=n) { if (!init_mode) logmsg(0,"%s",(n?t:f)); o=n; chg++; } }

#define CHECK_STATUS_CHANGE(t,v,chg,tmp,tmsg,fmsg) {		   \
    if (v != t) {						   \
      v=t;							   \
      chg++;							   \
      if (tmp[0]) strcat(tmp,", ");				   \
      strcat (tmp,(t ? tmsg : fmsg));				   \
    }								   \
  }



void process_message(nxmsg_t *msg, int init_mode, int verbose_mode, nx_system_status_t *astat, nx_interface_status_t *istatus) 
{
  unsigned char msgnum;
  int i;

  if (!msg) return;

  if (verbose_mode) nx_print_msg(stdout,msg);

  msgnum = msg->msgnum & NX_MSG_MASK;

  switch (msgnum) {

  case NX_INT_CONFIG_MSG:
    memcpy(istatus->version,&msg->msg[0],4);
    istatus->version[4]=0;
    memcpy(istatus->sup_trans_msgs,&msg->msg[4],2);
    memcpy(istatus->sup_cmd_msgs,&msg->msg[6],4);
    break;

  case NX_ZONE_NAME_MSG:
    {
      int zone = msg->msg[0];
      if (astat->zones[zone].valid) {
	memcpy(astat->zones[zone].name,&msg->msg[1],16);
	astat->zones[zone].name[16]=0;
      }
    }
    break;

  case NX_ZONE_STATUS_MSG:
    {
      int zonenum = msg->msg[0];

      if (zonenum >= astat->last_zone) break;

      if (astat->zones[zonenum].valid) {
	int fault = (msg->msg[5] & 0x01 ? 1:0);
	int bypass = (msg->msg[5] & 0x08 ? 1:0);
	int trouble = (msg->msg[5] & 0x66 ? 1:0);
	int alarm_mem = (msg->msg[6] & 0x01 ? 1:0);
	char tmp[255];
	nx_zone_status_t *zone = &astat->zones[zonenum];
	int change=0;
	int change2=0;

	tmp[0]=0;

	CHECK_STATUS_CHANGE(fault,zone->fault,change,tmp,"Fault","Ok");
	CHECK_STATUS_CHANGE(bypass,zone->bypass,change2,tmp,"Bypass","(Bypass)");
	CHECK_STATUS_CHANGE(trouble,zone->trouble,change,tmp,"Trouble","(Trouble)");
	CHECK_STATUS_CHANGE(alarm_mem,zone->alarm_mem,change,tmp,"Alarm Memory","(Alarm Memory)");

	if (change && !init_mode) {
	  logmsg(0,"%s zone status: %02d %s: %s",
		  (zone->bypass ? "bypassed" : (astat->armed ? "armed" : "normal")),
		  zonenum+1,
		  zone->name,
		  tmp);
	  zone->last_updated=msg->r_time;
	}
	if (zone->last_updated <=0) zone->last_updated=msg->r_time;
      }
    }
    break;


  case NX_ZONE_SNAPSHOT_MSG:
    {
      int offset = msg->msg[0];
      int zonenum;

      for (zonenum=offset; zonenum<offset+16; zonenum++) {
	if (astat->zones[zonenum].valid) {
	  char tmp[255];
	  int change = 0;
	  int fault, bypass, trouble, alarm_mem;
	  nx_zone_status_t *zone = &astat->zones[zonenum];
	  uchar s = msg->msg[1+((zonenum-offset)/2)];

	  if (zonenum % 2 == 1) s = s >> 4;
	  fault = (s & 0x01 ? 1:0);
	  bypass = (s & 0x02 ? 1:0);
	  trouble = (s & 0x04 ? 1:0);
	  alarm_mem = (s & 0x08 ? 1:0);

	  tmp[0]=0;

	  CHECK_STATUS_CHANGE(fault,zone->fault,change,tmp,"Fault","Ok");
	  CHECK_STATUS_CHANGE(bypass,zone->bypass,change,tmp,"Bypass","(Bypass)");
	  CHECK_STATUS_CHANGE(trouble,zone->trouble,change,tmp,"Trouble","(Trouble)");
	  CHECK_STATUS_CHANGE(alarm_mem,zone->alarm_mem,change,tmp,"Alarm Memory","(Alarm Memory)");
	  
	  if (change) {
	    logmsg(0,"%s zone status (snapshot): %02d %s: %s",
		   (zone->bypass ? "bypassed" : (astat->armed ? "armed" : "normal")),
		   zonenum+1,
		   zone->name,
		   tmp);
	    zone->last_updated=msg->r_time;
	  }
	}
      }
    }
    break;


  case NX_PART_STATUS_MSG:
    {
      int partnum = msg->msg[0];

      if (partnum >= astat->last_partition) break;
      if (astat->partitions[partnum].valid) {
	char tmp[1024];
	nx_partition_status_t *part = &astat->partitions[partnum];
	int change = 0;
	int p,armed_count;

	tmp[0]=0;
	
	/* armed */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x40?1:0),part->armed,change,tmp,"Armed","Not Armed");
	/* ready */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x04?1:0),part->ready,change,tmp,"Ready","Not Ready");

	/* stay mode */
	CHECK_STATUS_CHANGE((msg->msg[3]&0x04?1:0),part->stay_mode,change,tmp,"Stay Mode","(Stay Mode)");
	/* chime mode */
	CHECK_STATUS_CHANGE((msg->msg[3]&0x08?1:0),part->chime_mode,change,tmp,"Chime Mode","(Chime Mode)");
	/* entry delay */
	CHECK_STATUS_CHANGE((msg->msg[3]&0x10?1:0),part->entry_delay,change,tmp,"Entry Delay","(Entry Delay)");
	/* exit delay */
	CHECK_STATUS_CHANGE((msg->msg[3]&0x40?1:0),part->exit_delay,change,tmp,"Exit Delay","(Exit Delay)");
	/* previous alarm */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x01?1:0),part->prev_alarm,change,tmp,"Previous Alarm","(Previous Alarm)");

	/* fire */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x04?1:0),part->fire,change,tmp,"Fire","(Fire)");
	/* fire trouble */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x02?1:0),part->fire_trouble,change,tmp,"Fire Trouble","(Fire Trouble)");
	/* instant */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x80?1:0),part->instant,change,tmp,"Instant","(Instant)");
	/* tamper */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x10?1:0),part->tamper,change,tmp,"Tamper","(Tamper)");
	/* valid pin */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x10?1:0),part->valid_pin,change,tmp,"Valid PIN","(Valid PIN)");
	/* cancel entered */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x20?1:0),part->cancel_entered,change,tmp,"Cancel Entered","(Cancel Entered)");
	/* code entered */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x40?1:0),part->code_entered,change,tmp,"Code Entered","(Code Entered)");

	/* buzzer on */
	CHECK_STATUS_CHANGE((msg->msg[1]&0x08?1:0),part->buzzer_on,change,tmp,"Buzzer On","(Buzzer On)");
	/* siren on */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x02?1:0),part->siren_on,change,tmp,"Siren On","(Siren On)");
	/* steady siren on */
	CHECK_STATUS_CHANGE((msg->msg[2]&0x04?1:0),part->steadysiren_on,change,tmp,"Steady Siren On","(Steady Siren On)");
	/* chime on */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x20?1:0),part->chime_on,change,tmp,"Chime On","(Chime On)");
	/* error beep on */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x40?1:0),part->errorbeep_on,change,tmp,"Error Beep","(Error Beep)");
	/* tone on */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x80?1:0),part->tone_on,change,tmp,"Tone On","(Tone On)");

	/* zones bypassed */
	CHECK_STATUS_CHANGE((msg->msg[6]&0x01?1:0),part->zones_bypassed,change,tmp,"Zones Bypassed","(Zones Bypassed)");
	

	/* last user */
	if (msg->msg[5] != part->last_user) {
	  char tstr[255];
	  part->last_user=msg->msg[5];
	  change++;
	  if (tmp[0]) strcat(tmp,", ");
	  snprintf(tstr,sizeof(tstr),"Last User = %03u",part->last_user);
	  strcat(tmp,tstr);
	}

	if (change && !init_mode) {
	  logmsg(0,"Partition %d status change: %s",partnum+1,tmp);
	  part->last_updated=msg->r_time;
	}

	/* update global armed flag... */
	armed_count=0;
	for(p=0;p < astat->last_partition;p++) {
	  if (astat->partitions[p].valid && astat->partitions[p].armed) armed_count++;
	}
	astat->armed=(armed_count > 0 ? 1:0);

      }
    }
    break;

  case NX_PART_SNAPSHOT_MSG:
    {
      int armed_count=0;

      for(i=0;i<NX_PARTITIONS_MAX;i++) {
	char tmp[255];
	unsigned char s = msg->msg[i];
	unsigned char v = (s & 0x01 ? 1:0);
	unsigned char t;
	nx_partition_status_t *part = &astat->partitions[i]; 
	int change = 0;
	
	if (part->valid != v) {
	  part->valid=v;
	  logmsg(1,"Partition %d %s",i+1,(v ? "Active":"Disabled"));
	}
	
	if (part->valid > 0) {
	  tmp[0]=0;
	  
	  /* armed */
	  t=(s & 0x04 ? 1:0);
	  if (t) armed_count++;
	  CHECK_STATUS_CHANGE(t,part->armed,change,tmp,"Armed","Not Armed");
	  
	  /* ready */
	  t=(s & 0x02 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->ready,change,tmp,"Ready","Not Ready");
	  
	  /* stay mode */
	  t=(s & 0x08 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->stay_mode,change,tmp,"Stay Mode","(Stay Mode)");
	  
	  /* chime mode */
	  t=(s & 0x10 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->chime_mode,change,tmp,"Chime Mode","(Chime Mode)");
	  
	  /* entry delay */
	  t=(s & 0x20 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->entry_delay,change,tmp,"Entry Delay","(Entry Delay)");
	  
	  /* exit delay */
	  t=(s & 0x40 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->exit_delay,change,tmp,"Exit Delay","(Exit Delay)");

	  /* previous alarm */
	  t=(s & 0x80 ? 1:0);
	  CHECK_STATUS_CHANGE(t,part->prev_alarm,change,tmp,"Previous Alarm","(Previous Alarm)");

	  
	  if (change) {
	    logmsg(0,"Partition %d status change: %s",i+1,tmp);
	    part->last_updated=msg->r_time;
	  }
	}
      }

      /* update global armed flag... */
      astat->armed=(armed_count > 0 ? 1:0);

    }
    break;

  case NX_SYS_STATUS_MSG:
    {
      int change = 0;

      if (astat->panel_id != (uchar) msg->msg[0]) {
	if (init_mode)
	  logmsg(0,"Panel ID: %d",msg->msg[0]);
	else
	  logmsg(0,"Panel ID change: %d -> %d",astat->panel_id,msg->msg[0]);
	astat->panel_id=msg->msg[0];
	change++;
      }
      if (astat->comm_stack_ptr != (uchar) msg->msg[10]) {
	logmsg(3,"panel communication stack pointer change: %d -> %d",astat->comm_stack_ptr,msg->msg[10]);
	astat->comm_stack_ptr=msg->msg[10];
	change++;
      }


      LOG_STATUS_CHANGE(astat->partitions[0].valid,(msg->msg[9]&0x01 ? 1:0),change,"Partition 1 Active","Partition 1 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[1].valid,(msg->msg[9]&0x02 ? 1:0),change,"Partition 2 Active","Partition 2 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[2].valid,(msg->msg[9]&0x04 ? 1:0),change,"Partition 3 Active","Partition 3 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[3].valid,(msg->msg[9]&0x08 ? 1:0),change,"Partition 4 Active","Partition 4 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[4].valid,(msg->msg[9]&0x10 ? 1:0),change,"Partition 5 Active","Partition 5 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[5].valid,(msg->msg[9]&0x20 ? 1:0),change,"Partition 6 Active","Partition 6 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[6].valid,(msg->msg[9]&0x40 ? 1:0),change,"Partition 7 Active","Partition 7 Disabled");
      LOG_STATUS_CHANGE(astat->partitions[7].valid,(msg->msg[9]&0x80 ? 1:0),change,"Partition 8 Active","Partition 8 Disabled");


      LOG_STATUS_CHANGE(astat->line_seizure,(msg->msg[1]&0x01 ? 1:0),change,"Line Seizure start","Line Seizure end");
      LOG_STATUS_CHANGE(astat->off_hook,(msg->msg[1]&0x02 ? 1:0),change,"Off Hook","On Hook");
      LOG_STATUS_CHANGE(astat->handshake_rcvd,(msg->msg[1]&0x04 ? 1:0),change,"Initial Handshake Received","(Initial Handshake Received)");
      LOG_STATUS_CHANGE(astat->download_in_progress,(msg->msg[1]&0x08 ? 1:0),change,"Download in progress","Download end");
      LOG_STATUS_CHANGE(astat->dialerdelay_in_progress,(msg->msg[1]&0x10 ? 1:0),change,"Dialer delay in progress","Dialer Delay end");
      LOG_STATUS_CHANGE(astat->backup_phone,(msg->msg[1]&0x20 ? 1:0),change,"Using backup phone","Using primary backup phone");
      LOG_STATUS_CHANGE(astat->listen_in,(msg->msg[1]&0x40 ? 1:0),change,"Listen in active","Listen in inactive");
      LOG_STATUS_CHANGE(astat->twoway_lockout,(msg->msg[1]&0x80 ? 1:0),change,"Two way lockout","(Two way lockout)");

      LOG_STATUS_CHANGE(astat->ground_fault,(msg->msg[2]&0x01 ? 1:0),change,"Ground Fault","Ground OK");
      LOG_STATUS_CHANGE(astat->phone_fault,(msg->msg[2]&0x02 ? 1:0),change,"Phone Fault","Phone OK");
      LOG_STATUS_CHANGE(astat->fail_to_comm,(msg->msg[2]&0x04 ? 1:0),change,"Fail to communicate","(Fail to communicate)");
      LOG_STATUS_CHANGE(astat->fuse_fault,(msg->msg[2]&0x08 ? 1:0),change,"Fuse Fault","Fuse OK");
      LOG_STATUS_CHANGE(astat->box_tamper,(msg->msg[2]&0x10 ? 1:0),change,"Box Tamper","(Box Tamper)");
      LOG_STATUS_CHANGE(astat->siren_tamper,(msg->msg[2]&0x20 ? 1:0),change,"Siren Tamper","(Siren Tamper)");
      LOG_STATUS_CHANGE(astat->low_battery,(msg->msg[2]&0x40 ? 1:0),change,"Low Battery","Battery OK");
      LOG_STATUS_CHANGE(astat->ac_fail,(msg->msg[2]&0x80 ? 1:0),change,"AC Fail","AC OK");

      LOG_STATUS_CHANGE(astat->exp_tamper,(msg->msg[3]&0x01 ? 1:0),change,"Expander box tamper","(Expander box tamper)");
      LOG_STATUS_CHANGE(astat->exp_ac_fail,(msg->msg[3]&0x02 ? 1:0),change,"Expander AC failure","Expander AC OK");
      LOG_STATUS_CHANGE(astat->exp_low_battery,(msg->msg[3]&0x04 ? 1:0),change,"Expander battery LOW","Expander battery OK");
      LOG_STATUS_CHANGE(astat->exp_loss_supervision,(msg->msg[3]&0x08 ? 1:0),change,"Expander loss of supervision","Expander supervision restored");
      LOG_STATUS_CHANGE(astat->exp_aux_overcurrent,(msg->msg[3]&0x10 ? 1:0),change,"Expander aux ouput overcurrent","(Expander aux output overcurrent)");
      LOG_STATUS_CHANGE(astat->aux_com_channel_fail,(msg->msg[3]&0x20 ? 1:0),change,"Aux communication channel FAILURE","Aux communication channel OK");
      LOG_STATUS_CHANGE(astat->exp_bell_fault,(msg->msg[3]&0x40 ? 1:0),change,"Expander bell fault","Expander bell OK");
      
      LOG_STATUS_CHANGE(astat->sixdigitpin,(msg->msg[4]&0x01 ? 1:0),change,"6 Digit PIN enabled","4 Digit PIN enabled");
      LOG_STATUS_CHANGE(astat->prog_token_inuse,(msg->msg[4]&0x02 ? 1:0),change,"Programming token in use","(Programming token in use)");
      LOG_STATUS_CHANGE(astat->pin_local_dl,(msg->msg[4]&0x04 ? 1:0),change,"PIN required for local download","PIN not required for local download");
      LOG_STATUS_CHANGE(astat->global_pulsing_buzzer,(msg->msg[4]&0x08 ? 1:0),change,"Global pulsing buzzer ON","Global pulsing buzzer OFF");
      LOG_STATUS_CHANGE(astat->global_siren,(msg->msg[4]&0x10 ? 1:0),change,"Global siren ON","Global siren OFF");
      LOG_STATUS_CHANGE(astat->global_steady_siren,(msg->msg[4]&0x20 ? 1:0),change,"Global steady siren ON","Global steady siren OFF");
      LOG_STATUS_CHANGE(astat->bus_seize_line,(msg->msg[4]&0x40 ? 1:0),change,"Bus device has line seized","(Bus device has line seized)");
      LOG_STATUS_CHANGE(astat->bus_sniff_mode,(msg->msg[4]&0x80 ? 1:0),change,"Bus device has requested sniff mode","(Bus device has requested sniff mode)");

      LOG_STATUS_CHANGE(astat->battery_test,(msg->msg[5]&0x01 ? 1:0),change,"Dynamic Battery Test start","Dynamic Battery Test end");
      LOG_STATUS_CHANGE(astat->ac_power,(msg->msg[5]&0x02 ? 1:0),change,"AC power ON","AC power OFF");
      LOG_STATUS_CHANGE(astat->low_battery_memory,(msg->msg[5]&0x04 ? 1:0),change,"Low battery memory","(Low battery memory)");
      LOG_STATUS_CHANGE(astat->ground_fault_memory,(msg->msg[5]&0x08 ? 1:0),change,"Ground fault memory","(Ground fault memory)");
      LOG_STATUS_CHANGE(astat->fire_alarm_verification,(msg->msg[5]&0x10 ? 1:0),change,"Fire alarm verification being timed","(Fire alarm verification being timed)");
      LOG_STATUS_CHANGE(astat->smoke_power_reset,(msg->msg[5]&0x20 ? 1:0),change,"Smoke detector power reset","(Smoke detector power reset)");
      LOG_STATUS_CHANGE(astat->line_power_50hz,(msg->msg[5]&0x40 ? 1:0),change,"50 Hz line power detected","60 Hz line power detected");
      LOG_STATUS_CHANGE(astat->high_voltage_charge,(msg->msg[5]&0x80 ? 1:0),change,"Timing a high voltage battery charge","(Timing a high voltage battery charge)");
      
      LOG_STATUS_CHANGE(astat->comm_since_autotest,(msg->msg[6]&0x01 ? 1:0),change,"Communication since last autotest","(Communication since last autotest)");
      LOG_STATUS_CHANGE(astat->powerup_delay,(msg->msg[6]&0x02 ? 1:0),change,"Power up delay in progress","Power up delay end");
      LOG_STATUS_CHANGE(astat->walktest_mode,(msg->msg[6]&0x04 ? 1:0),change,"Walk test mode ON","Walk test mode OFF");
      LOG_STATUS_CHANGE(astat->system_time_loss,(msg->msg[6]&0x08 ? 1:0),change,"Loss of system time","(Loss of system time)");
      LOG_STATUS_CHANGE(astat->enroll_request,(msg->msg[6]&0x10 ? 1:0),change,"Enroll request","(Enroll request)");
      LOG_STATUS_CHANGE(astat->testfixture_mode,(msg->msg[6]&0x20 ? 1:0),change,"Test fixture mode ON","Test fixture mode OFF");
      LOG_STATUS_CHANGE(astat->controlshutdown_mode,(msg->msg[6]&0x40 ? 1:0),change,"Control shutdown mode ON","Control shutdown mode OFF");
      LOG_STATUS_CHANGE(astat->cancel_window,(msg->msg[6]&0x80 ? 1:0),change,"Timing cancel window","(Timing cancel window)");
      
      LOG_STATUS_CHANGE(astat->callback_in_progress,(msg->msg[7]&0x80 ? 1:0),change,"Call back in progress","Call back end");
      
      LOG_STATUS_CHANGE(astat->phone_line_fault,(msg->msg[8]&0x01 ? 1:0),change,"Phone line Faulted","Phone line OK");
      LOG_STATUS_CHANGE(astat->voltage_present_int,(msg->msg[8]&0x02 ? 1:0),change,"Voltage present interrupt","(Voltage present interrupt)");
      LOG_STATUS_CHANGE(astat->house_phone_offhook,(msg->msg[8]&0x04 ? 1:0),change,"House phone OFF hook","House phone ON hook");
      LOG_STATUS_CHANGE(astat->phone_monitor,(msg->msg[8]&0x08 ? 1:0),change,"Phone line monitor enabled","Phone line monitor disabled");
      LOG_STATUS_CHANGE(astat->phone_sniffing,(msg->msg[8]&0x10 ? 1:0),change,"Phone sniffing","(Phone sniffing)");
      /* LOG_STATUS_CHANGE(astat->offhook_memory,(msg->msg[8]&0x20 ? 1:0),change,"Last read was off hook","(Last read was off hook)"); */
      LOG_STATUS_CHANGE(astat->listenin_request,(msg->msg[8]&0x40 ? 1:0),change,"Listen in requested","(Listen in requested)");
      LOG_STATUS_CHANGE(astat->listenin_trigger,(msg->msg[8]&0x80 ? 1:0),change,"Listen in trigger","(Listen in trigger)");
      
      if (change) astat->last_updated=msg->r_time;
    }
    break;


  case NX_LOG_EVENT_MSG:
    {
      nx_log_event_t *e;
      uchar num = msg->msg[0];
      uchar maxnum = msg->msg[1];

      if (astat->last_log != maxnum) {
	if (astat->last_log != 0)
	  logmsg(2,"panel log size change: %d -> %d",astat->last_log,maxnum);
	astat->last_log=maxnum;
      }
      
      /* save log message */
      e=&astat->log[num];
      e->msgno=msg->msgnum;
      e->no=num;
      e->logsize=maxnum;
      e->type=msg->msg[2];
      e->num=msg->msg[3];
      e->part=msg->msg[4];
      e->month=msg->msg[5];
      e->day=msg->msg[6];
      e->hour=msg->msg[7];
      e->min=msg->msg[8];
      
      logmsg((NX_IS_NONREPORTING_EVENT(e->type)?1:0),"%s",nx_log_event_str(e));
    }
    break;


  default:
    if (verbose_mode) fprintf(stderr,"%s: unhandled message 0x%X\n",nx_timestampstr(msg->r_time),msgnum); 
    logmsg(1,"unhandled message 0x%X",msgnum);
    
  }

}



void process_command(int fd, int protocol, const uchar *data, nx_interface_status_t *istatus)
{
  nxmsg_t msgout,msgin;
  int func,ret,len,offset;
  char *funcname = NULL;

  if (!data || !istatus) return;

  //logmsg(0,"process_command: %02x,%02x,%02x",data[0],data[1],data[2]);
  func=(data[0]<<8 | data[1]);

  switch (func) {
  case NX_KEYPAD_FUNC_STAY:
    funcname="Stay (one button arm / toggle interiors)";
    break;
  case NX_KEYPAD_FUNC_CHIME:
    funcname="Chime (toggle chime mode)";
    break;
  case NX_KEYPAD_FUNC_EXIT:
    funcname="Exit (one button arm / toggle instant)";
    break;
  case NX_KEYPAD_FUNC_BYPASS:
    funcname="Bypass interiors";
    break;
  case NX_KEYPAD_FUNC_GROUP_BYPASS:
    funcname="Group Bypass";
    break;
  case NX_KEYPAD_FUNC_SMOKE_RESET:
    funcname="Smoke detector reset";
    break;
  case NX_KEYPAD_FUNC_START_SOUNDER:
    funcname="Start keypad sounder";
    break;
  case NX_KEYPAD_FUNC_ARM_AWAY:
    funcname="Arm in Away mode";
    break;
  case NX_KEYPAD_FUNC_ARM_STAY:
    funcname="Arm in Stay mode";
    break;
  case NX_KEYPAD_FUNC_DISARM:
    funcname="Disarm partition";
    break;
  case NX_KEYPAD_FUNC_SILENCE:
    funcname="Turn off any sounder or alarm";
    break;
  case NX_KEYPAD_FUNC_CANCEL:
    funcname="Cancel (alarm)";
    break;
  case NX_KEYPAD_FUNC_AUTO_ARM:
    funcname="Initiate Auto-Arm";
    break;

  default:
    funcname=NULL;
  }

  if (!funcname) {
    logmsg(0,"Invalid command message received: 0x%04x",func);
    return;
  }


  /* check if required commands are enabled on the interface */
  if (data[0] == NX_PRI_KEYPAD_FUNC_PIN) {
    if ((istatus->sup_cmd_msgs[3] & 0x10) == 0) {
      logmsg(0,"Ignoring disabled command: Primary Keypad function with PIN command (0x3c)");
      //return;
    }
  } 
  else if (data[0] == NX_SEC_KEYPAD_FUNC) {
    if ((istatus->sup_cmd_msgs[3] & 0x40) == 0) {
      logmsg(0,"Ignoring disabled command: Secondary Keypad Function with PIN (0x3e)");
      return;
    }
  } 


  msgout.msgnum=data[0];
  if (data[0] == NX_PRI_KEYPAD_FUNC_PIN) {
    len=6;
    offset=3;
    msgout.msg[0]=data[3]; // PIN digits 1&2
    msgout.msg[1]=data[4]; // PIN digits 3&4
    msgout.msg[2]=data[5]; // PIN digits 5&6
  } else {
    len=3;
    offset=0;
  }
  msgout.len=len;
  msgout.msg[offset]=data[1]; // keypad function
  msgout.msg[offset+1]=data[2]; // partition mask


  logmsg(2,"sending keypad function command: %s (partitions=0x%02x)",funcname,data[2]);

  ret=nx_send_message(fd,protocol,&msgout,5,3,NX_POSITIVE_ACK,&msgin);
  if (ret == 1 && msgin.msgnum == NX_POSITIVE_ACK) {
    logmsg(1,"keypad function success (partitions=0x%02x): %s",data[2],funcname);
  } else if (ret == 1 && msgin.msgnum == NX_MSG_REJECTED) {
    logmsg(0,"keypad function rejected (partitions=0x%02x): %s",data[2],funcname);
  } else {
    logmsg(0,"keypad function failed (partitions=0x%02x,ret=%d,msg=0x%02x): %s",
	   data[2],ret,msgin.msgnum,funcname);
  }

}


/* eof :-) */
