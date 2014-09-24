import java.io.*;
import java.util.*;
import java.awt.*;
import javax.swing.*;
import java.awt.event.*;

// Socket
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.Socket;



public class EnergyViewer extends JFrame
{
        /// ======== VALUES =========

        class Values {

                public int id;

                /*public double cpu_usage;
                public double cpu_load;
                public double cpu_load_idle;
                public double cpu_load_sys;
                public double cpu_load_user;
                public double cpu_load_comp;
                public double kflops;
                */

                public String cpu_usage;
                public String cpu_load;
                public String cpu_load_idle;
                public String cpu_load_sys;
                public String cpu_load_user;
                public String cpu_load_comp;
                public String frequency;
                public String kflops;
                public String delta_time;
                public String delta_energy;

                // fields that are updated locally
                public double time_start;
                public double time_end;
                public double time_last;
                public double energy;
                public double energy_idle;
                public double energy_sys;
                public double energy_user;
                public double energy_comp;

                Values() {
                        cpu_usage = new String();
                        cpu_load = new String();
                        cpu_load_idle = new String();
                        cpu_load_sys = new String();
                        cpu_load_user = new String();
                        cpu_load_comp = new String();
                        frequency = new String();
                        kflops = new String();
                        delta_time = new String();
                        delta_energy = new String();
                }

                
                public void clear() {
                        cpu_usage = "";
                        cpu_load = "";
                        cpu_load_idle = "";
                        cpu_load_sys = "";
                        cpu_load_user = "";
                        cpu_load_comp = "";
                        frequency = "";
                        kflops = "";
                        delta_time = "";
                        delta_energy = "";
                }

                public void print() {

                        System.out.println("Values.print():");
                        System.out.println("Id: " + this.id);
                        System.out.println("CpuUsage: " + this.cpu_usage);
                        System.out.println("Cpuload: " + this.cpu_load);
                        System.out.println("CpuLoadIdle: " + cpu_load_idle);
                        System.out.println("CpuLoadSys: " + cpu_load_sys);
                        System.out.println("CpuLoadUser: " + cpu_load_user);
                        System.out.println("CpuLoadComp: " + cpu_load_comp);
                        System.out.println("Frequency: " + frequency);
                        System.out.println("Kflops: " + kflops);
                        System.out.println("DeltaT: " + delta_time);
                        System.out.println("DeltaEnergy: " + delta_energy);
                }

                /*
                public void read(String csvStr) {
                        String[] v = csvStr.split(";");

                        System.out.println("Id: " + v[Value.Id.val()]);
                        System.out.println("CpuUsage: " + v[Value.CpuUsage.val()]);
                        System.out.println("Cpuload: " + v[Value.CpuLoad.val()]);
                        System.out.println("CpuLoadIdle: " + v[Value.CpuLoadIdle.val()]);
                        System.out.println("CpuLoadSys: " + v[Value.CpuLoadSys.val()]);
                        System.out.println("CpuLoadUser: " + v[Value.CpuLoadUser.val()]);
                        System.out.println("CpuLoadComp: " + v[Value.CpuLoadComp.val()]);
                        //System.out.println("Kflops: " + v[Value.Kflops.val()]);

                        this.id = Integer.parseInt(v[Value.Id.val()]);
               }*/

        }
        
        /// ======== VIEWER =========

        class Viewer extends JPanel
        {

                public Values [] map;

                private static final int step_x = 50;

                boolean first = true;

                Viewer()
                {

                        map = new Values[10];
                        for (int i = 0; i < 10; i++) {
                                map[i] = new Values();
                        }
                }

                protected void paintComponent(Graphics g)
                {
                        Graphics2D g2 = (Graphics2D) g;
                        
                        //if (first) {
                                RenderingHints rh;
                                
                                rh = new RenderingHints(RenderingHints.KEY_TEXT_ANTIALIASING,
                                                        RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
                                g2.setRenderingHints(rh);       
                                rh = new RenderingHints(RenderingHints.KEY_ANTIALIASING,
                                                        RenderingHints.VALUE_ANTIALIAS_ON);
                                g2.setRenderingHints(rh);
                                first = false;
                                //}

                        // Don't know why "- 30"...
                        //g2.translate(0, getHeight());
                        //g2.scale(1, -1);

                        g2.setColor(new Color(200, 200, 200, 255));
                        g2.fillRect(0, 0, getWidth(), getHeight());

                        int xc = getWidth() / 2;
                        int yc = getHeight() / 2;
                        int r = xc / 2;
                        
                        g2.setColor(new Color(255, 0, 0, 200));
                        g2.fillOval(xc - r, yc - r, 2 * r, 2 * r);	
 
                        g.setColor(Color.black);
                        g.drawString("TEST", 150, 150);
                        
                        // Apply left margin

                        g.translate(margin, 0);

                        // Draw Values

                        drawValuesArray(g2);
                }

                private void drawValuesArray(Graphics2D g) {


                        //g.translate(0, 0);
                        g.scale(4, 4);

                        g.translate(0, 20);
                        // Draw "Header"
                        g.drawString("slot", step_x * 0, 0);
                        g.drawString("usage", step_x * 1, 0);
                        g.drawString("load", step_x * 2, 0);
                        g.drawString("idle", step_x * 3, 0);
                        g.drawString("sys", step_x * 4, 0);
                        g.drawString("user", step_x * 5, 0);
                        g.drawString("comp", step_x * 6, 0);
                        g.drawString("freq", step_x * 7, 0);
                        g.drawString("deltaT", step_x * 8, 0);
                        g.drawString("deltaE", step_x * 9, 0);
                        
                        for (int i = 0; i < 10; i++) {
                                Values v = map[i];
                                g.translate(0, 20);
                                drawValues(g, v);
                        }
                }

                private void drawValues(Graphics2D g, Values v) {

                        if (v != null) {

                                g.drawString(String.valueOf(v.id), step_x* 0, 0);
                                g.drawString(v.cpu_usage, step_x* 1, 0);
                                g.drawString(v.cpu_load, step_x* 2, 0);
                                g.drawString(v.cpu_load_idle, step_x* 3, 0);
                                g.drawString(v.cpu_load_sys, step_x* 4, 0);
                                g.drawString(v.cpu_load_user, step_x* 5, 0);
                                g.drawString(v.cpu_load_comp, step_x* 6, 0);
                                g.drawString(v.frequency, step_x* 7, 0);
                                g.drawString(v.delta_time, step_x* 8, 0);
                                g.drawString(v.delta_energy, step_x* 9, 0);
                        }
                }


                public void read(String csvStr) {
                        
                        System.out.println("READ:");
                        String[] s = csvStr.split(";");
                        
                        int id = Integer.parseInt(s[Value.Id.val()]);
                        //Values v = map.get(id);
                        Values v = map[id];
                        
                        v.clear();
                
                        v.id = id;//Integer.parseInt(s[Value.Id.val()]);
                        
                        System.out.println("Id: " + v.id);
                        System.out.println("CpuUsage: " + s[Value.CpuUsage.val()]);
                        System.out.println("Cpuload: " + s[Value.CpuLoad.val()]);
                        System.out.println("CpuLoadIdle: " + s[Value.CpuLoadIdle.val()]);
                        System.out.println("CpuLoadSys: " + s[Value.CpuLoadSys.val()]);
                        System.out.println("CpuLoadUser: " + s[Value.CpuLoadUser.val()]);
                        System.out.println("CpuLoadComp: " + s[Value.CpuLoadComp.val()]);
                        //System.out.println("Kflops: " + s[Value.Kflops.val()]);
                        
                        v.cpu_usage = s[Value.CpuUsage.val()];
                        v.cpu_load = s[Value.CpuLoad.val()];
                        v.cpu_load_idle = s[Value.CpuLoadIdle.val()];
                        v.cpu_load_sys = s[Value.CpuLoadSys.val()];
                        v.cpu_load_user = s[Value.CpuLoadUser.val()];
                        v.cpu_load_comp = s[Value.CpuLoadComp.val()];
                        v.frequency = s[Value.Frequency.val()];
                        //v.kflops = s[Value.Kflops.val()];
                        v.delta_time = s[Value.DeltaTime.val()];
                        v.delta_energy = s[Value.DeltaEnergy.val()];
                        
                        v.print();
                        
                        /*
                          System.out.println("Cpuload: " + v[Value.CpuLoad.val()]);
                          System.out.println("CpuLoadIdle: " + v[Value.CpuLoadIdle.val()]);
                          System.out.println("CpuLoadSys: " + v[Value.CpuLoadSys.val()]);
                          System.out.println("CpuLoadUser: " + v[Value.CpuLoadUser.val()]);
                          System.out.println("CpuLoadComp: " + v[Value.CpuLoadComp.val()]);
                          System.out.println("Kflops: " + v[Value.Kflops.val()]);
                        */
                        //map.put(id, v);
                }
                
        }

        public static final int frame_width = 980;
        public static final int frame_height = 720;

        public static final int graph_width = 640;
        public static final int graph_height = 480;
        public static final int margin = 30;

        //public Map<Integer, Values> map;

        public Values [] map;

        Viewer viewer;

        EnergyViewer() throws Exception 
        {
                viewer = new Viewer();
                add(viewer, BorderLayout.CENTER);

                setExtendedState(Frame.MAXIMIZED_BOTH);  
                setUndecorated(true); 
                pack();
                setVisible(true);
                
                /*
                map = new Map<Integer, Values>();
                for (int i = 0; i < 10; i++) {
                        map.put(i, new Values() );
                }
                */
                map = new Values[10];
                for (int i = 0; i < 10; i++) {
                        map[i] = new Values();
                }
        }
        

        
        
        public void paint() {
                
                Graphics2D g = (Graphics2D) viewer.getGraphics();
                
                this.clear(g);

                /*
                for (int i = 0; i < 10; i++) {
                  //Values v = map[i];
                        
                  g.translate(i*20, i*20);
                  g.drawString("TEST", 150, 180);
                        
                }
                

                //viewer.updateValues( ,map);
                */

                viewer.drawValuesArray(g);
        }

        public void repaint() {
                viewer.repaint();
        }

        public void clear(Graphics2D g) {
                g.clearRect(0, 0, frame_width, frame_height);
                g.setColor(new Color(255, 255, 255, 255));
                g.fillRect(0, 0, getWidth(), getHeight());
                
        }
        
        public enum Value
        {
                Name(0), Id(1), CpuUsage(2), CpuLoad(3), CpuLoadIdle(4), CpuLoadSys(5), CpuLoadUser(6), CpuLoadComp(7), Frequency(8), DeltaTime(9), DeltaEnergy(10), NumValue(11);

                private int num;
                private Value(int i) {
                        num = i;
                }
                public int val() {
                        return num;
                }
        }

        public void read(String str) {
                viewer.read(str);
        }

        public static void main(String[] args) throws Exception
        {
                EnergyViewer ev = new EnergyViewer();

                System.out.println("TEST !!!");
                //viewer.updateValues();
                
                Socket socket1;
                int portNumber = 2015;
                String str = "initialize";
                
                socket1 = new Socket(InetAddress.getLocalHost(), portNumber);
                
                BufferedReader br = new BufferedReader(new InputStreamReader(socket1.getInputStream()));
                
                //PrintWriter pw = new PrintWriter(socket1.getOutputStream(), true);
                
                //pw.println(str);
                //ValueReader vr = new EnergyViewer.ValueReader();
                
                String[] str_array = new String[3];

                str_array[0] = ("data;0;3.3;4.4;5.5;6.6;7.7;8.8;8.9;10.10;11.11;");
                str_array[1] = ("data;0;4.3;4.4;5.5;6.6;7.7;8.8;3.9;10.10;11.11;");
                str_array[2] = ("data;1;5.3;4.4;5.5;6.6;7.7;8.8;5.9;10.10;11.11;");

                System.out.println("before loop");
                int i = 0;
                //while (true) {
                while ((str = br.readLine()) != null) {
                        System.out.println("in loop test");
                        System.out.println(str);
                        
                        
                        //String[] v = str.split(";");
                        
                        //vr.clear();
                        ev.read(str);
                        //ev.read(str_array[i]);
                        //ev.read("date;data;1;3.3;4.4;5.5;6.6;7.7;8.8;9.9;10.10;11.11;");
                        //ev.paint();

                        //pw.println("bye");
                  
                        //if (str.equals("bye"))
                        //break;
                        //System.out.println("in while");
                        /*
                          if (i == 2) {
                                i = 0;
                        } else {
                                i++;
                        }
                        */
                        ev.repaint();
                        try {
 
                               Thread.sleep(1000);
                        } catch(InterruptedException e) {
                                // this is executed when an exception (in this example InterruptedException) occurs
                        }

                }
                
                //System.out.println("close...");
                //br.close();
                //socket1.close();
                /*
                  
                        */
                
                //}
                
                
                
        }
}

