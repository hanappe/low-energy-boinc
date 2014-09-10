import java.io.*;
import java.util.*;
import java.awt.*;
import javax.swing.*;
import java.awt.event.*;

public class EnergyViewer extends JFrame
{
        class Viewer extends JPanel
        {
                boolean first = true;

                Viewer()
                {
                }

                protected void paintComponent(Graphics g)
                {
                        Graphics2D g2 = (Graphics2D) g;
                        
                        if (first) {
                                RenderingHints rh;
                                
                                rh = new RenderingHints(RenderingHints.KEY_TEXT_ANTIALIASING,
                                                        RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
                                g2.setRenderingHints(rh);       
                                rh = new RenderingHints(RenderingHints.KEY_ANTIALIASING,
                                                        RenderingHints.VALUE_ANTIALIAS_ON);
                                g2.setRenderingHints(rh);

                                first = false;
                        }

                        g2.setColor(new Color(255, 255, 255, 255));
                        g2.fillRect(0, 0, getWidth(), getHeight());

                        int xc = getWidth() / 2;
                        int yc = getHeight() / 2;
                        int r = xc / 2;
                        
                        g2.setColor(new Color(255, 0, 0, 200));
                        g2.fillOval(xc - r, yc - r, 2 * r, 2 * r);	
                }
        }

        EnergyViewer() throws Exception 
        {
                Viewer viewer = new Viewer();
                add(viewer, BorderLayout.CENTER);

 
                setExtendedState(Frame.MAXIMIZED_BOTH);  
                setUndecorated(true); 
                pack();
                setVisible(true);  
        }
        
        public static void main(String[] args) throws Exception
        {
                new EnergyViewer();
        }
}

