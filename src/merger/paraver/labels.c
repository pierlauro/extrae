/*****************************************************************************\
 *                        ANALYSIS PERFORMANCE TOOLS                         *
 *                                   Extrae                                  *
 *              Instrumentation package for parallel applications            *
 *****************************************************************************
 *     ___     This library is free software; you can redistribute it and/or *
 *    /  __         modify it under the terms of the GNU LGPL as published   *
 *   /  /  _____    by the Free Software Foundation; either version 2.1      *
 *  /  /  /     \   of the License, or (at your option) any later version.   *
 * (  (  ( B S C )                                                           *
 *  \  \  \_____/   This library is distributed in hope that it will be      *
 *   \  \__         useful but WITHOUT ANY WARRANTY; without even the        *
 *    \___          implied warranty of MERCHANTABILITY or FITNESS FOR A     *
 *                  PARTICULAR PURPOSE. See the GNU LGPL for more details.   *
 *                                                                           *
 * You should have received a copy of the GNU Lesser General Public License  *
 * along with this library; if not, write to the Free Software Foundation,   *
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA          *
 * The GNU LEsser General Public License is contained in the file COPYING.   *
 *                                 ---------                                 *
 *   Barcelona Supercomputing Center - Centro Nacional de Supercomputacion   *
\*****************************************************************************/

/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- *\
 | @file: $HeadURL$
 | @last_commit: $Date$
 | @version:     $Revision$
\* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
#include "common.h"

static char UNUSED rcsid[] = "$Id$";

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_TIME_H
# include <time.h>
#endif
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "events.h"
#include "labels.h"
#include "mpi_prv_events.h"
#include "pacx_prv_events.h"
#include "omp_prv_events.h"
#include "trt_prv_events.h"
#include "cuda_prv_events.h"
#include "pthread_prv_events.h"
#include "misc_prv_events.h"
#include "misc_prv_semantics.h"
#include "trace_mode.h"
#include "addr2info.h" 
#include "mpi2out.h"
#include "options.h"

#include "HardwareCounters.h"
#include "queue.h"

static codelocation_label_t *labels_codelocation = NULL;
static unsigned num_labels_codelocation = 0;

static void Labels_Add_CodeLocation_Label (int eventcode, codelocation_type_t type, char *description)
{
	labels_codelocation = (codelocation_label_t*) realloc (labels_codelocation, (num_labels_codelocation+1)*sizeof(codelocation_label_t));
	if (labels_codelocation == NULL)
	{
		fprintf (stderr, PACKAGE_NAME": mpi2prv Error! Cannot allocate memory to add a new code location label\n");
		exit (-1);
	}

	labels_codelocation[num_labels_codelocation].eventcode = eventcode;
	labels_codelocation[num_labels_codelocation].type = type;
	labels_codelocation[num_labels_codelocation].description = strdup (description);
	if (labels_codelocation[num_labels_codelocation].description == NULL)
	{
		fprintf (stderr, PACKAGE_NAME": mpi2prv Error! Cannot allocate memory to duplicate a code location label\n");
		exit (-1);
	}

	num_labels_codelocation++;
}

typedef struct label_hw_counter_st
{
	int eventcode;
	char *description;
} label_hw_counter_t;
static label_hw_counter_t *labels_hw_counters = NULL;
static unsigned num_labels_hw_counters = 0;

static void Labels_AddHWCounter_Code_Description (int eventcode, char *description)
{
	labels_hw_counters = (label_hw_counter_t*) realloc (labels_hw_counters, (num_labels_hw_counters+1)*sizeof(label_hw_counter_t));
	if (labels_hw_counters == NULL)
	{
		fprintf (stderr, PACKAGE_NAME": mpi2prv Error! Cannot allocate memory to add a hardware counter description\n");
		exit (-1);
	}

	labels_hw_counters[num_labels_hw_counters].eventcode = eventcode;
	labels_hw_counters[num_labels_hw_counters].description = strdup (description);
	if (labels_hw_counters[num_labels_hw_counters].description == NULL)
	{
		fprintf (stderr, PACKAGE_NAME": mpi2prv Error! Cannot allocate memory to duplicate hardware counter description\n");
		exit (-1);
	}

	num_labels_hw_counters++;
}

int Labels_LookForHWCCounter (int eventcode, unsigned *position, char **description)
{
	unsigned u;

	for (u = 0; u < num_labels_hw_counters; u++)
		if (labels_hw_counters[u].eventcode == eventcode)
		{
			*position = u;
			if (description != NULL)
				*description = labels_hw_counters[u].description;
			return TRUE;
		}

	return FALSE;
}

struct color_t states_inf[STATES_NUMBER] = {
  {STATE_0, STATE0_LBL, STATE0_COLOR},
  {STATE_1, STATE1_LBL, STATE1_COLOR},
  {STATE_2, STATE2_LBL, STATE2_COLOR},
  {STATE_3, STATE3_LBL, STATE3_COLOR},
  {STATE_4, STATE4_LBL, STATE4_COLOR},
  {STATE_5, STATE5_LBL, STATE5_COLOR},
  {STATE_6, STATE6_LBL, STATE6_COLOR},
  {STATE_7, STATE7_LBL, STATE7_COLOR},
  {STATE_8, STATE8_LBL, STATE8_COLOR},
  {STATE_9, STATE9_LBL, STATE9_COLOR},
  {STATE_10, STATE10_LBL, STATE10_COLOR},
  {STATE_11, STATE11_LBL, STATE11_COLOR},
  {STATE_12, STATE12_LBL, STATE12_COLOR},
  {STATE_13, STATE13_LBL, STATE13_COLOR},
  {STATE_14, STATE14_LBL, STATE14_COLOR},
  {STATE_15, STATE15_LBL, STATE15_COLOR},
  {STATE_16, STATE16_LBL, STATE16_COLOR},
  {STATE_17, STATE17_LBL, STATE17_COLOR}
};

struct color_t gradient_inf[GRADIENT_NUMBER] = {
  {GRADIENT_0, GRADIENT0_LBL, GRADIENT0_COLOR},
  {GRADIENT_1, GRADIENT1_LBL, GRADIENT1_COLOR},
  {GRADIENT_2, GRADIENT2_LBL, GRADIENT2_COLOR},
  {GRADIENT_3, GRADIENT3_LBL, GRADIENT3_COLOR},
  {GRADIENT_4, GRADIENT4_LBL, GRADIENT4_COLOR},
  {GRADIENT_5, GRADIENT5_LBL, GRADIENT5_COLOR},
  {GRADIENT_6, GRADIENT6_LBL, GRADIENT6_COLOR},
  {GRADIENT_7, GRADIENT7_LBL, GRADIENT7_COLOR},
  {GRADIENT_8, GRADIENT8_LBL, GRADIENT8_COLOR},
  {GRADIENT_9, GRADIENT9_LBL, GRADIENT9_COLOR},
  {GRADIENT_10, GRADIENT10_LBL, GRADIENT10_COLOR},
  {GRADIENT_11, GRADIENT11_LBL, GRADIENT11_COLOR},
  {GRADIENT_12, GRADIENT12_LBL, GRADIENT12_COLOR},
  {GRADIENT_13, GRADIENT13_LBL, GRADIENT13_COLOR},
  {GRADIENT_14, GRADIENT14_LBL, GRADIENT14_COLOR}
};

struct rusage_evt_t rusage_evt_labels[RUSAGE_EVENTS_COUNT] = {
   { RUSAGE_UTIME_EV, RUSAGE_UTIME_LBL },
   { RUSAGE_STIME_EV, RUSAGE_STIME_LBL },
   { RUSAGE_MAXRSS_EV,   RUSAGE_MAXRSS_LBL },
   { RUSAGE_IXRSS_EV,    RUSAGE_IXRSS_LBL },
   { RUSAGE_IDRSS_EV,    RUSAGE_IDRSS_LBL },
   { RUSAGE_ISRSS_EV,    RUSAGE_ISRSS_LBL },
   { RUSAGE_MINFLT_EV,   RUSAGE_MINFLT_LBL },
   { RUSAGE_MAJFLT_EV,   RUSAGE_MAJFLT_LBL },
   { RUSAGE_NSWAP_EV,    RUSAGE_NSWAP_LBL },
   { RUSAGE_INBLOCK_EV,  RUSAGE_INBLOCK_LBL },
   { RUSAGE_OUBLOCK_EV,  RUSAGE_OUBLOCK_LBL },
   { RUSAGE_MSGSND_EV,   RUSAGE_MSGSND_LBL },
   { RUSAGE_MSGRCV_EV,   RUSAGE_MSGRCV_LBL },
   { RUSAGE_NSIGNALS_EV, RUSAGE_NSIGNALS_LBL },
   { RUSAGE_NVCSW_EV,    RUSAGE_NVCSW_LBL },
   { RUSAGE_NIVCSW_EV,   RUSAGE_NIVCSW_LBL }
};

struct memusage_evt_t memusage_evt_labels[MEMUSAGE_EVENTS_COUNT] = {
   { MEMUSAGE_ARENA_EV, MEMUSAGE_ARENA_LBL },
   { MEMUSAGE_HBLKHD_EV, MEMUSAGE_HBLKHD_LBL },
   { MEMUSAGE_UORDBLKS_EV, MEMUSAGE_UORDBLKS_LBL },
   { MEMUSAGE_FORDBLKS_EV, MEMUSAGE_FORDBLKS_LBL },
   { MEMUSAGE_INUSE_EV, MEMUSAGE_INUSE_LBL }
};

struct mpi_stats_evt_t mpi_stats_evt_labels[MPI_STATS_EVENTS_COUNT] = {
   { MPI_STATS_P2P_COMMS_EV, MPI_STATS_P2P_COMMS_LBL },
   { MPI_STATS_P2P_BYTES_SENT_EV, MPI_STATS_P2P_BYTES_SENT_LBL },
   { MPI_STATS_P2P_BYTES_RECV_EV, MPI_STATS_P2P_BYTES_RECV_LBL },
   { MPI_STATS_GLOBAL_COMMS_EV, MPI_STATS_GLOBAL_COMMS_LBL },
   { MPI_STATS_GLOBAL_BYTES_SENT_EV, MPI_STATS_GLOBAL_BYTES_SENT_LBL },
   { MPI_STATS_GLOBAL_BYTES_RECV_EV, MPI_STATS_GLOBAL_BYTES_RECV_LBL },
   { MPI_STATS_TIME_IN_MPI_EV, MPI_STATS_TIME_IN_MPI_LBL }
};

struct pacx_stats_evt_t pacx_stats_evt_labels[PACX_STATS_EVENTS_COUNT] = {
   { PACX_STATS_P2P_COMMS_EV, PACX_STATS_P2P_COMMS_LBL },
   { PACX_STATS_P2P_BYTES_SENT_EV, PACX_STATS_P2P_BYTES_SENT_LBL },
   { PACX_STATS_P2P_BYTES_RECV_EV, PACX_STATS_P2P_BYTES_RECV_LBL },
   { PACX_STATS_GLOBAL_COMMS_EV, PACX_STATS_GLOBAL_COMMS_LBL },
   { PACX_STATS_GLOBAL_BYTES_SENT_EV, PACX_STATS_GLOBAL_BYTES_SENT_LBL },
   { PACX_STATS_GLOBAL_BYTES_RECV_EV, PACX_STATS_GLOBAL_BYTES_RECV_LBL },
   { PACX_STATS_TIME_IN_PACX_EV, PACX_STATS_TIME_IN_PACX_LBL }
};

/******************************************************************************
 ***  state_labels
 ******************************************************************************/
static void Paraver_state_labels (FILE * fd)
{
  int i;

  fprintf (fd, "%s\n", STATES_LBL);
  for (i = 0; i < STATES_NUMBER; i++)
  {
    fprintf (fd, "%d    %s\n", states_inf[i].value, states_inf[i].label);
  }

  LET_SPACES (fd);
}


/******************************************************************************
 ***  state_colors
 ******************************************************************************/
static void Paraver_state_colors (FILE * fd)
{
  int i;

  fprintf (fd, "%s\n", STATES_COLOR_LBL);
  for (i = 0; i < STATES_NUMBER; i++)
  {
    fprintf (fd, "%d    {%d,%d,%d}\n", states_inf[i].value,
             states_inf[i].rgb[0], states_inf[i].rgb[1],
             states_inf[i].rgb[2]);
  }

  LET_SPACES (fd);
}

/******************************************************************************
 ***  gradient_colors
 ******************************************************************************/
static void Paraver_gradient_colors (FILE * fd)
{
  int i;

  fprintf (fd, "%s\n", GRADIENT_COLOR_LBL);
  for (i = 0; i < GRADIENT_NUMBER; i++)
  {
    fprintf (fd, "%d    {%d,%d,%d}\n", gradient_inf[i].value,
             gradient_inf[i].rgb[0],
             gradient_inf[i].rgb[1], gradient_inf[i].rgb[2]);
  }

  LET_SPACES (fd);
}

/******************************************************************************
 ***  gradient_names
 ******************************************************************************/
static void Paraver_gradient_names (FILE * fd)
{
  int i;

  fprintf (fd, "%s\n", GRADIENT_LBL);
  for (i = 0; i < GRADIENT_NUMBER; i++)
    fprintf (fd, "%d    %s\n", gradient_inf[i].value, gradient_inf[i].label);

  LET_SPACES (fd);
}

/******************************************************************************
 *** concat_user_labels
 ******************************************************************************/
static void Concat_User_Labels (FILE * fd)
{
	char *str;
	char line[1024];
	FILE *labels;

	if ((str = getenv ("EXTRAE_LABELS")) != NULL)
	{
		labels = fopen (str, "r");
		if (labels == NULL)
		{
			fprintf (stderr, "mpi2prv: Cannot open file %s (pointed by EXTRAE_LABELS)\n",
			  labels);
      return;
		}

		fprintf (fd, "\n");
		while (fscanf (labels, "%[^\n]\n", line) != EOF)
		{
			if (strlen (line) == 0)
			{
				line[0] = fgetc (labels);
				fprintf (fd, "%s\n", line);
				continue;
			}
			fprintf (fd, "%s\n", line);
		}
		fclose (labels);
		fprintf (fd, "\n");
	}
}

/******************************************************************************
 *** PARAVER_default_options
 ******************************************************************************/
static void Paraver_default_options (FILE * fd)
{
	fprintf (fd, "DEFAULT_OPTIONS\n\n");
	fprintf (fd, "LEVEL               %s\n", DEFAULT_LEVEL);
	fprintf (fd, "UNITS               %s\n", DEFAULT_UNITS);
	fprintf (fd, "LOOK_BACK           %d\n", DEFAULT_LOOK_BACK);
	fprintf (fd, "SPEED               %d\n", DEFAULT_SPEED);
	fprintf (fd, "FLAG_ICONS          %s\n", DEFAULT_FLAG_ICONS);
	fprintf (fd, "NUM_OF_STATE_COLORS %d\n", DEFAULT_NUM_OF_STATE_COLORS);
	fprintf (fd, "YMAX_SCALE          %d\n", DEFAULT_YMAX_SCALE);

	LET_SPACES (fd);

	fprintf (fd, "DEFAULT_SEMANTIC\n\n");
	fprintf (fd, "THREAD_FUNC          %s\n", DEFAULT_THREAD_FUNC);

	LET_SPACES (fd);
}

#if USE_HARDWARE_COUNTERS
static int Exist_Counter (fcounter_t *fcounter, long long EvCnt) 
{
  struct fcounter_t *aux_fc = fcounter;

  while (aux_fc != NULL)
  {
    if (aux_fc->counter == EvCnt)
      return 1;
    else
      aux_fc = aux_fc->prev;
  }
  return 0;
}


/******************************************************************************
 *** HWC_PARAVER_Labels
******************************************************************************/

static void HWC_PARAVER_Labels (FILE * pcfFD)
{
#if defined(PAPI_COUNTERS)
  struct fcounter_t *fcounter=NULL;
#elif defined(PMAPI_COUNTERS)
	pm_info2_t ProcessorMetric_Info; /* On AIX pre 5.3 it was pm_info_t */
	pm_groups_info_t HWCGroup_Info;
	pm_events2_t *evp = NULL;
	int j;
	int rc;
#endif
	int cnt = 0;
	int AddedCounters = 0;
	CntQueue *queue;
	CntQueue *ptmp;

#if defined(PMAPI_COUNTERS)
	rc = pm_initialize (PM_VERIFIED|PM_UNVERIFIED|PM_CAVEAT|PM_GET_GROUPS, &ProcessorMetric_Info, &HWCGroup_Info, PM_CURRENT);
	if (rc != 0)
		pm_error ("pm_initialize", rc);
#endif

	queue = &CountersTraced;

	for (ptmp = (queue)->prev; ptmp != (queue); ptmp = ptmp->prev)
	{
		for (cnt = 0; cnt < MAX_HWC; cnt++)
		{
			if (ptmp->Traced[cnt])
			{
#if defined(PAPI_COUNTERS)
				if (!Exist_Counter(fcounter,ptmp->Events[cnt]))
				{
					unsigned position;
					char *description;

					INSERTAR_CONTADOR (fcounter, ptmp->Events[cnt]);

					if (Labels_LookForHWCCounter (ptmp->Events[cnt], &position, &description))
					{
						if (AddedCounters == 0)
							fprintf (pcfFD, "%s\n", TYPE_LABEL);
						AddedCounters++;

						/* fprintf (pcfFD, "%d  %d %s\n", 7, HWC_COUNTER_TYPE(position), description); */
						fprintf (pcfFD, "%d  %d %s\n", 7, HWC_COUNTER_TYPE(ptmp->Events[cnt]), description);
					}
				}
#elif defined(PMAPI_COUNTERS)
				/* find pointer to the event */
				for (j = 0; j < ProcessorMetric_Info.maxevents[cnt]; j++)
				{ 
					evp = ProcessorMetric_Info.list_events[cnt]+j;  
					if (EvCnt == evp->event_id)
						break;    
				}
				if (evp != NULL)
				{
					if (AddedCounters == 0)
						fprintf (pcfFD, "%s\n", TYPE_LABEL);

					fprintf (pcfFD, "%d  %lld %s (%s)\n", 7, (long long)HWC_COUNTER_TYPE(cnt, EvCnt), evp->short_name, evp->long_name);
					AddedCounters++;
				}
#endif
			}
		}
	}

	if (AddedCounters > 0)
		fprintf (pcfFD, "%d  %d %s\n", 7, HWC_GROUP_ID, "Active hardware counter set");

	LET_SPACES (pcfFD);
}
#endif

static char * Rusage_Event_Label (int rusage_evt) {
   int i;

   for (i=0; i<RUSAGE_EVENTS_COUNT; i++) {
      if (rusage_evt_labels[i].evt_type == rusage_evt) {
         return rusage_evt_labels[i].label;
      }
   }
   return "Unknown getrusage event";
}

static void Write_rusage_Labels (FILE * pcf_fd)
{
   int i;

   if (Rusage_Events_Found) {
      fprintf (pcf_fd, "%s\n", TYPE_LABEL);

      for (i=0; i<RUSAGE_EVENTS_COUNT; i++) {
         if (GetRusage_Labels_Used[i]) {
            fprintf(pcf_fd, "0    %d    %s\n", RUSAGE_BASE+i, Rusage_Event_Label(i));
         }
      }
      LET_SPACES (pcf_fd);
   }
}

static char * Memusage_Event_Label (int memusage_evt) {
	int i;
	
	for (i=0; i<MEMUSAGE_EVENTS_COUNT; i++) {
		if (memusage_evt_labels[i].evt_type == memusage_evt) {
			return memusage_evt_labels[i].label;
		}
	}
	return "Unknown memusage event";
}

static void Write_memusage_Labels (FILE * pcf_fd)
{
   int i;

   if (Memusage_Events_Found) {
      fprintf (pcf_fd, "%s\n", TYPE_LABEL);

      for (i=0; i<MEMUSAGE_EVENTS_COUNT; i++) {
         if (Memusage_Labels_Used[i]) {
            fprintf(pcf_fd, "0    %d    %s\n", MEMUSAGE_BASE+i, Memusage_Event_Label(i));
         }
      }
      LET_SPACES (pcf_fd);
   }
}

static char * MPI_Stats_Event_Label (int mpi_stats_evt)
{
   int i;

   for (i=0; i<MPI_STATS_EVENTS_COUNT; i++)
   {
      if (mpi_stats_evt_labels[i].evt_type == mpi_stats_evt) {
         return mpi_stats_evt_labels[i].label;
      }
   }
   return "Unknown MPI stats event";
}

static void Write_MPI_Stats_Labels (FILE * pcf_fd)
{
   int i;

   if (MPI_Stats_Events_Found)
   {
      fprintf (pcf_fd, "%s\n", TYPE_LABEL);

      for (i=0; i<MPI_STATS_EVENTS_COUNT; i++) {
         if (MPI_Stats_Labels_Used[i]) {
            fprintf(pcf_fd, "0    %d    %s\n", MPI_STATS_BASE+i, MPI_Stats_Event_Label(i));
         }
      }
      LET_SPACES (pcf_fd);
   }
}

static char * PACX_Stats_Event_Label (int pacx_stats_evt)
{
   int i;

   for (i=0; i<MPI_STATS_EVENTS_COUNT; i++)
   {
      if (pacx_stats_evt_labels[i].evt_type == pacx_stats_evt) {
         return pacx_stats_evt_labels[i].label;
      }
   }
   return "Unknown PACX stats event";
}

static void Write_PACX_Stats_Labels (FILE * pcf_fd)
{
   int i;

   if (PACX_Stats_Events_Found)
   {
      fprintf (pcf_fd, "%s\n", TYPE_LABEL);

      for (i=0; i<PACX_STATS_EVENTS_COUNT; i++) {
         if (PACX_Stats_Labels_Used[i]) {
            fprintf(pcf_fd, "0    %d    %s\n", PACX_STATS_BASE+i, PACX_Stats_Event_Label(i));
         }
      }
      LET_SPACES (pcf_fd);
   }
}

static void Write_Trace_Mode_Labels (FILE * pcf_fd)
{
	fprintf (pcf_fd, "%s\n", TYPE_LABEL);
	fprintf (pcf_fd, "9    %d    %s\n", TRACING_MODE_EV, "Tracing mode:");
	fprintf (pcf_fd, "%s\n", VALUES_LABEL);
	fprintf (pcf_fd, "%d      %s\n", TRACE_MODE_DETAIL, "Detailed");
	fprintf (pcf_fd, "%d      %s\n", TRACE_MODE_BURSTS, "CPU Bursts");
	LET_SPACES (pcf_fd);
}

static void Write_Clustering_Labels (FILE * pcf_fd)
{
	int i;

	if (MaxClusterId > 0)
	{
		fprintf (pcf_fd, "%s\n", TYPE_LABEL);
		fprintf (pcf_fd, "9    %d    %s\n", CLUSTER_ID_EV, CLUSTER_ID_LABEL);
		fprintf (pcf_fd, "%s\n", VALUES_LABEL);
		fprintf (pcf_fd, "0   End\n");
		fprintf (pcf_fd, "1   Missing Data\n");
		fprintf (pcf_fd, "2   Duration Filtered\n");
		fprintf (pcf_fd, "3   Range Filtered\n");
		fprintf (pcf_fd, "4   Threshold Filtered\n");
		fprintf (pcf_fd, "5   Noise\n");
		for (i=6; i<=MaxClusterId; i++)
		{
			fprintf (pcf_fd, "%d   Cluster %d\n", i, i-5);
		}
		LET_SPACES (pcf_fd);
	}
}

/******************************************************************************
 *** Labels_loadSYMfile
 ******************************************************************************/
void Labels_loadSYMfile (int taskid, char *name)
{
	FILE *FD;
	char LINE[1024], Type;
	int function_count = 0, hwc_count = 0;

	if (!name)
		return;

	if (strlen(name) == 0)
		return;

	FD = (FILE *) fopen (name, "r");
	if (FD == NULL)
	{
		fprintf (stderr, PACKAGE_NAME"mpi2prv: WARNING: Task %d Can\'t find file %s\n", taskid, name);
		return;
	}

	while (!feof (FD))
	{
		int args_assigned;

		if (fgets (LINE, 1024, FD) == NULL)
			break;

		args_assigned = sscanf (LINE, "%c %[^\n]", &Type, LINE);

		if (args_assigned == 2)
		{
			switch (Type)
			{
				case 'U':
				case 'P':
					{
#ifdef HAVE_BFD
						/* Example of line: U 0x100016d4 fA mpi_test.c 0 */
						char fname[1024], modname[1024];
						int line;
						UINT64 address;

						sscanf (LINE, "%llx %s %s %d", &address, fname, modname, &line);
						if (get_option_merge_UniqueCallerID())
							Address2Info_AddSymbol (address, UNIQUE_TYPE, fname, modname, line);
						else
							Address2Info_AddSymbol (address, (Type=='U')?USER_FUNCTION_TYPE:OUTLINED_OPENMP_TYPE, fname, modname, line);
						function_count++;
#endif /* HAVE_BFD */
					}
					break;

				case 'H':
					{
						int eventcode;
						char hwc_description[1024];

						sscanf (LINE, "%d %[^\n]", &eventcode, hwc_description);
						Labels_AddHWCounter_Code_Description (eventcode, hwc_description);
						hwc_count++;
					}
					break;

				case 'c':
				case 'C':
					{
						int eventcode;
						char code_description[1024];

						sscanf (LINE, "%d %[^\n]", &eventcode, code_description);
						Labels_Add_CodeLocation_Label (eventcode,
							Type=='C'?CODELOCATION_FUNCTION:CODELOCATION_FILELINE,
							code_description);
					}
					break;

				default:
					fprintf (stderr, PACKAGE_NAME" mpi2prv: Error! Task %d found unexpected line in symbol file '%s'\n", taskid, LINE);
					break;
			}
		}
	}

	if (taskid == 0)
	{
		fprintf (stdout, "mpi2prv: %d symbols successfully imported from %s file\n", function_count, name);
		fprintf (stdout, "mpi2prv: %d HWC counter descriptions successfully imported from %s file\n", hwc_count, name);
	}

	fclose (FD);
}

/******************************************************************************
 *** generatePCFfile
 ******************************************************************************/

int Labels_GeneratePCFfile (char *name, long long options)
{
	FILE *fd;

	fd = fopen (name, "w");
	if (fd == NULL)
		return -1;

	Paraver_default_options (fd);

	Paraver_state_labels (fd);
	Paraver_state_colors (fd);

	MPITEvent_WriteEnabled_MPI_Operations (fd);
	MPITEvent_WriteEnabled_PACX_Operations (fd);
	SoftCountersEvent_WriteEnabled_MPI_Operations (fd);
	SoftCountersEvent_WriteEnabled_PACX_Operations (fd);
	OMPEvent_WriteEnabledOperations (fd);
	pthreadEvent_WriteEnabledOperations (fd);
	MISCEvent_WriteEnabledOperations (fd, options);
	TRTEvent_WriteEnabledOperations (fd);
	CUDAEvent_WriteEnabledOperations (fd);

#if USE_HARDWARE_COUNTERS
	HWC_PARAVER_Labels (fd);
#endif

	Paraver_gradient_colors (fd);
	Paraver_gradient_names (fd);

#ifdef HAVE_BFD
	Address2Info_Write_MPI_Labels (fd, get_option_merge_UniqueCallerID());
	Address2Info_Write_UF_Labels (fd, get_option_merge_UniqueCallerID());
	Address2Info_Write_Sample_Labels (fd, get_option_merge_UniqueCallerID());
	Address2Info_Write_CUDA_Labels (fd, get_option_merge_UniqueCallerID());
	Address2Info_Write_OTHERS_Labels (fd, get_option_merge_UniqueCallerID(),
		num_labels_codelocation, labels_codelocation);
#endif

	Write_rusage_Labels (fd);
	Write_memusage_Labels (fd);
	Write_MPI_Stats_Labels (fd);
	Write_PACX_Stats_Labels (fd);
	Write_Trace_Mode_Labels (fd);
	Write_Clustering_Labels (fd);

	Concat_User_Labels (fd);

	fclose(fd);
    
	return 0;
}
