import { AfterViewInit, Component } from '@angular/core';
import { NgModule } from '@angular/core';
import { CommonModule } from '@angular/common';



@Component({
  selector: 'app-features',
  standalone: true,
  imports: [CommonModule],
  templateUrl: './features.component.html',
  styleUrl: './features.component.scss'
})
export class FeaturesComponent implements AfterViewInit {

  features = [
    {
      icon: '/images/feature-icon-01.svg',
      title: 'Effortless Task Organization',
      description: 'Easily manage your workload with intuitive task creation, assignment, and prioritization tools. Our system ensures your team stays focused on what matters most.'
    },
      {
      icon: '/images/feature-icon-02.svg',
      title: 'Real-Time Collaboration',
      description: 'Keep your team on the same page with seamless communication and updates in real time, ensuring everyone works in sync.'
    },
    {
      icon: '/images/feature-icon-03.svg',
      title: 'Customizable Workflows',
      description: 'Tailor your workflow to match your business processes. Whether you are agile or traditional, our flexible system adapts to your needs.'
    },
    {
      icon: '/images/feature-icon-04.svg',
      title: 'Deadline Tracking',
      description: 'Stay ahead with automated deadline reminders. Ensure every project is delivered on time without unnecessary stress.'
    },
    {
      icon: '/images/feature-icon-05.svg',
      title: 'Insights and Analytics',
      description: 'Make informed decisions with detailed analytics. Measure productivity, track progress, and gain insights to optimize team performance.'
    },
    {
      icon: '/images/feature-icon-06.svg',
      title: 'Cross-Platform Access',
      description: 'Access your tasks and projects seamlessly across devices. From desktop to mobile, stay productive wherever you go.'
    },
    
  ];
  ngAfterViewInit() {
    const observer = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          entry.target.classList.add('in-view');
        } else {
          entry.target.classList.remove('in-view');
        }
      });
    });

    document.querySelectorAll('.animate-on-scroll').forEach((element) => {
      observer.observe(element);
    });
  }
}
